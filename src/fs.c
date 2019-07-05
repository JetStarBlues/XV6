// File system implementation. Five layers:
//   + Blocks      : allocator for raw disk blocks.
//   + Log         : crash recovery for multi-step updates.
//   + Files       : inode allocator, reading, writing, metadata.
//   + Directories : inode with special contents (list of other inodes!)
//   + Names       : paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines. The (higher-level) system call implementations
// are in sysfile.c.

/* The xv6 file system addresses the following challenges:
     . support for crash recovery
     . support for different processes operating on the fs at the same time
     . in-memory cache of popular blocks, because disk access is slow

   Seven layers ??
     . file descriptor : Abstracts many Unix resources (ex. pipes, devices,
                          files) using the fs interface
     . pathname        : Provides hiearchical path names like "/usr/bin/cats"
                          and resolves them with recursive lookup
     . directory       : Implements directories as a special kind of inode
                          whose content is a sequence of directory entries,
                          each of which contains a file's name and i-number
     . inode           : Provides individual files, each represented as an
                          inode with a unique i-number and some blocks
                          holding the file's data
     . logging         : Allows updates to several blocks to be wrapped into
                          a transaction. Ensures that the blocks in a
                          transaction are updated atomically in the face of
                          crashes (i.e. all are updated or none)
     . buffer cache    : Caches disk blocks and synchronizes access to them,
                          making sure that only one kernel process at a time
                          can modify the data stored in any particular block
     . disk            : Reads and writes blocks to a disk


   Disk contents:
 
       -------
       boot     block 0  // Why? If fs.img and xv6.img (bootblock + kernel) are separate
       -------
       super    block 1
       -------
       log      block 2..w
       ...
       -------
       inodes   block w..x
       ...
       -------
       bitmap   block x..y
       ...
       -------
       data     block y..z
       ...
       -------
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

// There should be one superblock per disk device, but we run with
// only one device
struct superblock sb; 

// Read the super block.
void readsb ( int dev, struct superblock* sb )
{
	struct buf* bp;

	bp = bread( dev, 1 );

	memmove( sb, bp->data, sizeof( *sb ) );

	brelse( bp );
}


// _________________________________________________________________________________

// Blocks

/* The block allocator maintains a bitmap of free blocks.
   One bit represents a block. If set the corresponding block is
   allocted, if clear it's free.
*/

// Zero a block.
static void bzero ( int dev, int blocknum )
{
	struct buf* bp;

	bp = bread( dev, blocknum );

	memset( bp->data, 0, BLOCKSIZE );

	log_write( bp );

	brelse( bp );
}

// Allocate a zeroed disk block
static uint balloc ( uint dev )
{
	struct buf* buffer;
	int         bmapstart,  // (0, 1, 2, ...) * BITS_PER_BLOCK
	            bitidx,
	            bitmask,
	            blocknum;

	buffer = 0;

	// For every bitmap block in the fs
	for ( bmapstart = 0; bmapstart < sb.size; bmapstart += BITS_PER_BLOCK )
	{
		// Read the bitmap block (buffer->data)
		buffer = bread( dev, BBLOCK( bmapstart, sb ) );

		// For every bit in the bitmap block
		for ( bitidx = 0; bitidx < BITS_PER_BLOCK; bitidx += 1 )
		{
			blocknum = bmapstart + bitidx;

			if ( blocknum >= sb.size )
			{
				/* Accounts for possibility that fs size is not a multiple
				   multiple of BLOCKSIZE. Break loop instead of evaluating
				   non-existent blocks.
				*/
				break;
			}

			bitmask = 1 << ( bitidx % 8 );

			// If the corresponding block is free, allocate it
			if ( ( buffer->data[ bitidx / 8 ] & bitmask ) == 0 )
			{
				buffer->data[ bitidx / 8 ] |= bitmask;  // Mark block as allocated

				log_write( buffer );  // Write the bitmap changes to disk

				brelse( buffer );

				bzero( dev, blocknum );  // Zero the block

				return blocknum;
			}
		}

		brelse( buffer );
	}

	panic( "balloc: out of blocks" );
}

// Free a disk block
static void bfree ( int dev, uint blocknum )
{
	struct buf* buffer;
	int         bitidx,
	            bitmask;

	readsb( dev, &sb );

	buffer = bread( dev, BBLOCK( blocknum, sb ) );

	bitidx = blocknum % BITS_PER_BLOCK;

	bitmask = 1 << ( bitidx % 8 );

	if ( ( buffer->data[ bitidx / 8 ] & bitmask ) == 0 )
	{
		panic( "bfree: freeing free block" );
	}

	buffer->data[ bitidx / 8 ] &= ~ bitmask;  // Mark block as free

	log_write( buffer );  // Write the bitmap changes to disk

	brelse( buffer );
}


// =================================================================================

// Inodes
//
// An inode describes a single unnamed file.
// The inode disk structure holds metadata: the file's type,
// its size, the number of links referring to it, and the
// list of blocks holding the file's content.
//
// The inodes are laid out sequentially on disk at
// sb.startinode. Each inode has a number, indicating its
// position on the disk.
//
// The kernel keeps a cache of in-use inodes in memory
// to provide a place for synchronizing access
// to inodes used by multiple processes. The cached
// inodes include book-keeping information that is
// not stored on disk: ip->ref and ip->valid.
//
// An inode and its in-memory representation go through a
// sequence of states before they can be used by the
// rest of the file system code.
//
// * Allocation: an inode is allocated if its type (on disk)
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry (open
//   files and current directories). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information (type, size, &c) in an inode
//   cache entry is only correct when ip->valid is 1.
//   ilock() reads the inode from
//   the disk and sets ip->valid, while iput() clears
//   ip->valid if ip->ref has fallen to zero.
//
// * Locked: file system code may only examine and modify
//   the information in an inode and its content if it
//   has first locked the inode.
//
// Thus a typical sequence is:
//   ip = iget( dev, inum )
//   ilock( ip )
//   ... examine and modify ip->xxx ...
//   iunlock( ip )
//   iput( ip )
//
// ilock() is separate from iget() so that system calls can
// get a long-term reference to an inode (as for an open file)
// and only lock it for short periods (e.g., in read()).
// The separation also helps avoid deadlock and races during
// pathname lookup. iget() increments ip->ref so that the inode
// stays cached and pointers to it remain valid.
//
// Many internal file system functions expect the caller to
// have locked the inodes involved; this lets callers create
// multi-step atomic operations.
//
// The icache.lock spin-lock protects the allocation of icache
// entries. Since ip->ref indicates whether an entry is free,
// and ip->dev and ip->inum indicate which i-node an entry
// holds, one must hold icache.lock while using any of those fields.
//
// An ip->lock sleep-lock protects all ip-> fields other than ref,
// dev, and inum.  One must hold ip->lock in order to
// read or write that inode's ip->valid, ip->size, ip->type, &c.

/* The term "inode" can refer to:
     . The on-disk data structure ("struct dinode") containing the
       file's size and list of data blocks
     . The in-memory data structure ("struct inode") which contains a
       copy of the on-disk inode as well as extra information
       (ex. inode number, reference count) needed within the kernel

   The on-disk inodes are packed into a contiguous area of disk
   called the "inode blocks".
   Every inode is the same size, so it is easy, given a number n
   ("inode number"), to find the nth inode on the disk.

   The kernel keeps an inode in memory only if there are C pointers
   referring to the in-memory inode.
   If the reference count reaches zero, the inode is removed from memory.
   'iget' (ref += 1) and 'iput' (ref -= 1) functions acquire and
   release pointers to an inode, modifying the reference count.
   Pointers to an inode can come from file descriptors,
   current working directories, and transient kernel code? like exec.

   Fields:
     . dinode->type  : Distinguishes between files, directories, special devices.
                       A type of zero indicates that the on-disk inode is free
     . dinode->nlink : Number of directory entries that refer to the inode.
                       It is used to determine whether the on-disk inode and
                       its data structures should be freed.
     . dinode->size  : Number of (data?) bytes in the file
     . dinode->addrs : The array records the block numbers of the disk blocks
                       holding the file's data

     . inode->ref    : Tracks the number of C pointers referring to an in-memory
                       inode. If the count drops to zero, the inode is discarded
                       from memory.
     . inode->valid  : ...

   Locks (p.84):
     . icache->lock : ...
     . inode->lock  : Ensures exclusive access to the inode's fields and
                      its data blocks
    
   The "struct inode" returned by iget is guaranteed to be valid until the
   corresponding call to iput. That is:
     . the on-disk inode won't be deleted, and
     . the in-memory inode won't be discarded
   Many parts of the fs depend on this behaviour of iget to:
     . Hold long-term references to inodes (as open files and current directories)
     . Prevent races and deadlocks ?? in code that manipulates
       multiple inodes (such as pathname lookup)

   The inode returned by iget may not have any useful content.
   In order to ensure it holds a copy of the on-disk inode, code must call 'ilock'.
   ilock locks the inode and reads the inode from disk (if it has not
   already been read in).

   Separating acquisition of pointers from locking helps avoid deadlock
   in some situations, for ex. during directory lookup ??
   Multiple processes can hold a C pointer to an inode (returned by iget),
   but only one process can lock the inode at a time.

   Diagram of the inode block:
       | dinode(0)..dinode(IPB - 1) | dinode(IPB)..dinode(2*IPB - 1) | ... | dinode(...)..dinode(NINODE) |

   Diagram of the inode cache:
       | spinlock | inode(0) | inode(1) | ... | inode(NINODE) |
*/

// Inode cache
/* Holds in-memory copies of on-disk inodes.

   An inode is kept in the cache if its reference count is
   greater than zero. Otherwise, the cache entry is re-used
   for a different inode.

   It's real job is synchronizing access by multiple processes;
   caching is secondary...

   The cache is write-through...
   Code that modifies a cached inode must immediately write it
   to disk with 'iupdate'
*/
struct {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;

	struct inode    inode [ NINODE ];

} icache;


static struct inode* iget ( uint dev, uint inum );


void iinit ( int dev )
{
	int i = 0;

	initlock( &icache.lock, "icache" );

	for ( i = 0; i < NINODE; i += 1 )
	{
		initsleeplock( &icache.inode[ i ].lock, "inode" );  // can concat idx to make name unique
	}

	readsb( dev, &sb );

	cprintf( "superblock:\n" );
	cprintf( "    size (total blocks) %d\n",   sb.size                               );
	cprintf( "    ninodes             %d\n",   sb.ninodes                            );
	cprintf( "    ninodeblocks        %d\n",   ( sb.ninodes / INODES_PER_BLOCK ) + 1 );
	cprintf( "    nlogblocks          %d\n",   sb.nlogblocks                         );
	cprintf( "    ndatablocks         %d\n",   sb.ndatablocks                        );
	cprintf( "    logstart            %d\n",   sb.logstart                           );
	cprintf( "    inodestart          %d\n",   sb.inodestart                         );
	cprintf( "    bmapstart           %d\n\n", sb.bmapstart                          );
}


// _________________________________________________________________________________

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void iupdate ( struct inode* ip )
{
	struct buf*    buffer;
	struct dinode* diskinode;

	buffer = bread( ip->dev, IBLOCK( ip->inum, sb ) );

	// Note that 'diskinode' is a pointer to a region in "buffer->data"
	diskinode = ( struct dinode* ) buffer->data + ( ip->inum % INODES_PER_BLOCK );

	diskinode->type  = ip->type;
	diskinode->major = ip->major;
	diskinode->minor = ip->minor;
	diskinode->nlink = ip->nlink;
	diskinode->size  = ip->size;

	memmove( diskinode->addrs, ip->addrs, sizeof( ip->addrs ) );

	log_write( buffer );  // write changes to disk

	brelse( buffer );
}


// _________________________________________________________________________________

// Allocate an inode on device dev.
// Mark it as allocated by giving it type type.
// Returns an unlocked but allocated and referenced inode.
//
/* Used to allocate a new inode.
   Similar to balloc...
*/
struct inode* ialloc ( uint dev, short type )
{
	int            inum;
	struct buf*    buffer;
	struct dinode* diskinode;

	// For each inode
	for ( inum = 1; inum < sb.ninodes; inum += 1 )
	{
		// Read the inode block containing the inum
		buffer = bread( dev, IBLOCK( inum, sb ) );

		// Retrieve the dinode
		/* Note, because of C casting,
		   "buffer->data + 1" is now actually,
		   "buffer->data + sizeof( dinode )"
		*/
		diskinode = ( struct dinode* ) buffer->data + ( inum % INODES_PER_BLOCK );

		// Found a free inode
		if ( diskinode->type == 0 )
		{
			// Zero contents
			memset( diskinode, 0, sizeof( *diskinode ) );

			// Mark as allocated by writing the new type to disk
			diskinode->type = type;

			log_write( buffer );  // write changes to disk

			brelse( buffer );

			// Return an in-memory copy of the inode
			return iget( dev, inum );
		}

		brelse( buffer );
	}

	panic( "ialloc: no inodes" );
}


// _________________________________________________________________________________

// Increment reference count for ip.
// Returns ip to enable ip = idup( ip1 ) idiom.
struct inode* idup ( struct inode* ip )
{
	acquire( &icache.lock );

	ip->ref += 1;

	release( &icache.lock );

	return ip;
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
//
/* Looks through the inode cache for an active entry (ip->ref > 0)
   with the desired device and inode number.
   If it finds one, it returns a new reference to that inode ??

   As iget scans, it records the position of the first empty
   slot, which it uses if it needs to allocate a cache entry.
*/
static struct inode* iget ( uint dev, uint inum )
{
	struct inode* ip;
	struct inode* empty;

	acquire( &icache.lock );

	empty = 0;

	// Is the inode already cached
	for ( ip = &icache.inode[ 0 ]; ip < &icache.inode[ NINODE ]; ip += 1 )
	{
		if ( ( ip->ref > 0 ) && ( ip->dev == dev ) && ( ip->inum == inum ) )
		{
			ip->ref += 1;

			release( &icache.lock );

			return ip;
		}

		// Remember empty slot
		if ( empty == 0 && ip->ref == 0 )
		{
			empty = ip;
		}
	}

	// Recycle an inode cache entry
	if ( empty == 0 )
	{
		panic( "iget: no inodes" );
	}

	ip = empty;

	ip->dev   = dev;
	ip->inum  = inum;
	ip->ref   = 1;
	ip->valid = 0;

	release( &icache.lock );

	return ip;
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
//
/* Releases a C pointer to an inode by decrementing
   its reference count.

   If this is the last reference, the inode's slot in the
   cache is now free and can be re-used for a different inode.

   If this is the last reference and there are no links to the
   inode, then the inode and its data blocks must be freed.
   As such iput:
     . Calls itrunc to truncate the file to zero bytes, freeing
       the data blocks.
     . Sets the inode type to zero (unallocated) and writes the
       change to disk

   See lock explanation p.85


   ~~~ crashes ~~~

   iput doesn't truncate a file immediately when the link count of
   the file drops to zero, because some process might still hold
   a reference to the in-memory inode (ip->ref > 0).
   A process might still be reading and writing to the file, because
   it successfully opened it.
   If a crash happens before the last process closes the file descriptor,
   the file will be marked as allocated on-disk even though no
   directory entry points to it (dip->nlink == 0).

   There are two ways to handle this:
     . On recovery, after reboot, the fs can scan the whole fs for files
       that are marked allocated but have no directory entry pointing to
       them. If such a file exists, it is freed.
     . The FS can record on disk (ex. in the superblock), the inode
       number of a file whose link count drops to zero, but whose reference
       count isn't zero.
       If the FS successfully frees the file when its ref count reaches
       zero, it removes the inode number from the on-disk list.
       Otherwise, on recovery, the FS will free any file that is on the list.

    xv6 implements neither of these!
    This means that over time, xv6 runs the risk of running out of disk space.
*/
void iput ( struct inode* ip )
{
	int r;

	acquiresleep( &ip->lock );

	// Inode has no links
	if ( ip->valid && ip->nlink == 0 )
	{
		acquire( &icache.lock );

		r = ip->ref;

		release( &icache.lock );

		// Inode has no links and no other references: truncate and free.
		if ( r == 1 )
		{
			itrunc( ip );  // Free the inode's data blocks

			ip->type = 0;  // Mark inode as unallocated

			iupdate( ip );  // Write changes to disk

			ip->valid = 0;
		}
	}

	releasesleep( &ip->lock );


	// Decrement the reference count
	acquire( &icache.lock );

	ip->ref -= 1;

	release( &icache.lock );
}

// Common idiom: unlock, then put.
void iunlockput ( struct inode* ip )
{
	iunlock( ip );

	iput( ip );
}


// _________________________________________________________________________________

// Lock the given inode.
// Reads the inode from disk if necessary.
/* Code must lock the inode before reading or writing
   its metadata or data.

   Once ilock gains exclusive access to the inode, it reads
   the inode from disk (but more likely, the buffer cache)
   if needed
*/
void ilock ( struct inode* ip )
{
	struct buf    *buffer;
	struct dinode *diskinode;

	if ( ip == 0 || ip->ref < 1 )
	{
		panic( "ilock" );
	}

	// Acquire lock
	acquiresleep( &ip->lock );

	// Inode contents need to be read-in from disk
	if ( ip->valid == 0 )
	{
		buffer = bread( ip->dev, IBLOCK( ip->inum, sb ) );

		diskinode = ( struct dinode* ) buffer->data + ( ip->inum % INODES_PER_BLOCK );

		ip->type  = diskinode->type;
		ip->major = diskinode->major;
		ip->minor = diskinode->minor;
		ip->nlink = diskinode->nlink;
		ip->size  = diskinode->size;

		memmove( ip->addrs, diskinode->addrs, sizeof( ip->addrs ) );

		brelse( buffer );

		ip->valid = 1;

		if ( ip->type == 0 )
		{
			panic( "ilock: no type" );
		}
	}
}

// Unlock the given inode.
/* Causes any processes waiting on the sleeplock to wakeup.
*/
void iunlock ( struct inode* ip )
{
	if ( ip == 0 || ! holdingsleep( &ip->lock ) || ip->ref < 1 )
	{
		panic( "iunlock" );
	}

	releasesleep( &ip->lock );
}


// _________________________________________________________________________________

// Inode content (data)

// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[]. The next NINDIRECT blocks are
// listed in block ip->addrs[ NDIRECT ].
//
/* Diagram:

            ->  -----------------
           |    type
           |    -----------------
           |    major
           |    -----------------
           |    minor
           |    -----------------
           |    nlink
    dinode |    -----------------
           |    size
           |    -----------------
           |    addrs[0]           // data block 0
           |    -----------------
           |    addrs[1]           // data block 1
           |    -----------------
           |    ...
           |    -----------------
           |    addrs[NDIRECT-1]   // data block NDIRECT-1
           |    -----------------
           |    addrs[NDIRECT]     // indirect block
            ->  -----------------


            ->  ---------------------------------  <- addrs[NDIRECT]
           |    data block NDIRECT+0
           |    ---------------------------------
  indirect |    data block NDIRECT+1
  block    |    ---------------------------------
           |    ...
           |    ---------------------------------
           |    data block NDIRECT+(NINDIRECT-1)
            ->  ---------------------------------


   The first 6KB of a file can be loaded from blocks in the inode.
   (NDIRECT x BLOCKSIZE = 12 x 512 = 6KB)

   The next 64KB can only be loaded after consulting the indirect block.
   (NINDIRECT x BLOCKSIZE = 128 x 512 = 64KB)

   This is a good on-disk representation but a complex one for clients.
   bmap abstracts this representation for higher-level routines
   such as readi and writei.
*/

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
//
/* Returns the disk block number of the inode's nth data block.
   If the inode does not have such a block yet, bmap allocates one.

   I.e. a file's data blocks are allocated "on-demand"

   An ip->addrs[ nth ] entry of zero indicates that the data block
   (or indirect block) is not allocated.
*/
static uint bmap ( struct inode* ip, uint datablock_num )
{
	uint        datablock_addr;
	uint*       indirectblock;
	struct buf* buffer;

	// Get from direct blocks
	if ( datablock_num < NDIRECT )
	{
		datablock_addr = ip->addrs[ datablock_num ];

		// If not present, allocate data block
		if ( datablock_addr == 0 )
		{
			datablock_addr = balloc( ip->dev ); 

			ip->addrs[ datablock_num ] = datablock_addr;
		}

		return datablock_addr;
	}


	// Get from indirect block
	datablock_num -= NDIRECT;

	if ( datablock_num < NINDIRECT )
	{
		// Get indirect block
		datablock_addr = ip->addrs[ NDIRECT ];

		// If not present, allocate indirect block
		if ( datablock_addr == 0 )
		{
			datablock_addr = balloc( ip->dev );

			ip->addrs[ NDIRECT ] = datablock_addr;
		}


		// Get data block from indirect block
		buffer = bread( ip->dev, datablock_addr );  // Read contents of indirect block

		indirectblock = ( uint* ) buffer->data;

		datablock_addr = indirectblock[ datablock_num ];

		// If not present, allocate data block
		if ( datablock_addr == 0 )
		{
			datablock_addr = balloc( ip->dev );

			indirectblock[ datablock_num ] = datablock_addr;

			log_write( buffer );  // Write changes (to indirect block) to disk
		}

		brelse( buffer );

		return datablock_addr;
	}


	// datablock_num >= (NDIRECT + NINDIRECT)
	panic( "bmap: out of range" );
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).
//
/* Free's a file's data blocks, and resets the inode's size to zero.
*/

// static void itrunc ( struct inode* ip )
void itrunc ( struct inode* ip )  // JK - make public so can use for O_TRUNC...
{
	int         i;
	struct buf* buffer;
	uint*       indirectblock;

	// Free direct blocks
	for ( i = 0; i < NDIRECT; i += 1 )
	{
		if ( ip->addrs[ i ] )
		{
			bfree( ip->dev, ip->addrs[ i ] );

			ip->addrs[ i ] = 0;
		}
	}

	// Free blocks listed in indirect block
	if ( ip->addrs[ NDIRECT ] )
	{
		buffer = bread( ip->dev, ip->addrs[ NDIRECT ] );

		indirectblock = ( uint* ) buffer->data;

		for ( i = 0; i < NINDIRECT; i += 1 )
		{
			if ( indirectblock[ i ] )
			{
				bfree( ip->dev, indirectblock[ i ] );
			}
		}

		brelse( buffer );


		// Free the indirect block itself
		bfree( ip->dev, ip->addrs[ NDIRECT ] );

		ip->addrs[ NDIRECT ] = 0;
	}

	ip->size = 0;  // reset inode size

	iupdate( ip );  // write changes to disk
}


// _________________________________________________________________________________

// Read data from inode.
// Caller must hold ip->lock.
/* Read n bytes of the inode's data blocks to dst.

   Reads that start or cross EOF will return less
   bytes than requested.
*/
int readi ( struct inode* ip, char* dst, uint off, uint n )
{
	uint        nRead,
	            nReadTotal,
	            nYetToRead,
	            nMaxCanRead;
	struct buf* buffer;

	// The data for devices does not reside in the file system
	if ( ip->type == T_DEV )
	{
		if ( ip->major < 0 || ip->major >= NDEV || ! devsw[ ip->major ].read )
		{
			return - 1;
		}

		return devsw[ ip->major ].read( ip, dst, n );
	}

	// Read starts beyond EOF, or (n < 0)
	if ( ( off > ip->size ) || ( n < 0 ) )
	{
		return - 1;
	}

	// Read starts at or crosses EOF.
	// Return fewer bytes than requested.
	if ( off + n > ip->size )
	{
		n = ip->size - off;  // Read only the bytes from offset to EOF
	}

	// Copy data from inode data blocks to dst
	nRead      = 0;
	nReadTotal = 0;

	while ( nReadTotal < n )
	{
		// Read in data block containing the offset
		buffer = bread( ip->dev, bmap( ip, off / BLOCKSIZE ) );

		/* If we can read all remaining bytes from the current
		   data block, do so without reading more bytes than requested
		*/
		nYetToRead = n - nReadTotal;  // bytes yet to read

		nMaxCanRead = BLOCKSIZE - ( off % BLOCKSIZE );  // max number of bytes can read from current data block

		nRead = min( nMaxCanRead, nYetToRead );

		memmove( dst, buffer->data + ( off % BLOCKSIZE ), nRead );

		brelse( buffer );

		nReadTotal += nRead;
		off        += nRead;
		dst        += nRead;

		// cprintf( "%d : %d : %d : %d\n", n, nYetToRead, nRead, nReadTotal );
	}

	if ( nReadTotal != n )
	{
		panic( "readi: nReadTotal != n" );
	}

	return n;
}

// Write data to inode.
// Caller must hold ip->lock.
/* Write n bytes from src to inode's data blocks.

   Writes that start at or cross EOF will grow the file,
   up to the maximum file size.
*/
int writei ( struct inode* ip, char* src, uint off, uint n )
{
	uint        nWritten,
	            nWrittenTotal,
	            nYetToWrite,
	            nMaxCanWrite;
	struct buf* buffer;

	// The data for devices does not reside in the file system
	if ( ip->type == T_DEV )
	{
		if ( ip->major < 0 || ip->major >= NDEV || ! devsw[ ip->major ].write )
		{
			return - 1;
		}

		return devsw[ ip->major ].write( ip, src, n );
	}

	// Write starts beyond EOF, or (n < 0)
	if ( ( off > ip->size ) || ( n < 0 ) )
	{
		return - 1;
	}

	// Write grows beyond maximum file size
	if ( off + n > MAXFILESZ * BLOCKSIZE )
	{
		return - 1;
	}

	// Copy data from src to inode data blocks
	nWritten      = 0;
	nWrittenTotal = 0;

	while ( nWrittenTotal < n )
	{
		// Read in data block containing the offset
		buffer = bread( ip->dev, bmap( ip, off / BLOCKSIZE ) );

		/* If we can write all remaining bytes to the current
		   data block, do so without writing more bytes than requested
		*/
		nYetToWrite = n - nWrittenTotal;  // bytes yet to be written

		nMaxCanWrite = BLOCKSIZE - ( off % BLOCKSIZE );  // max number of bytes can write to current data block

		nWritten = min( nMaxCanWrite, nYetToWrite );

		memmove( buffer->data + ( off % BLOCKSIZE ), src, nWritten );

		log_write( buffer );  // write data block changes to disk

		brelse( buffer );

		nWrittenTotal += nWritten;
		off           += nWritten;
		src           += nWritten;

		// cprintf( "%d : %d : %d : %d\n", n, nYetToWrite, nWritten, nWrittenTotal );
	}

	if ( nWrittenTotal != n )
	{
		panic( "writei: nWrittenTotal != n" );
	}

	// Write grew the file so update its size
	if ( n > 0 && off > ip->size )
	{
		ip->size = off;

		iupdate( ip );  // write changes to disk
	}

	return n;
}


// _________________________________________________________________________________

// Copies the inode's metadata into the "stat structure"
// Caller must hold ip->lock.
void stati ( struct inode* ip, struct stat* st )
{
	st->dev   = ip->dev;
	st->ino   = ip->inum;
	st->type  = ip->type;
	st->nlink = ip->nlink;
	st->size  = ip->size;
}


// =================================================================================

// Directories

int namecmp ( const char* s, const char* t )
{
	return strncmp( s, t, DIRNAMESZ );
}

// Look for a directory entry in a directory.
// If found, set *poff to byte offset of entry.
struct inode* dirlookup ( struct inode* dp, char* name, uint* poff )
{
	uint          off,
	              inum;
	struct dirent de;

	if ( dp->type != T_DIR )
	{
		panic( "dirlookup: not DIR" );
	}

	for ( off = 0; off < dp->size; off += sizeof( de ) )
	{
		if ( readi( dp, ( char* ) &de, off, sizeof( de ) ) != sizeof( de ) )
		{
			panic( "dirlookup: read" );
		}

		if ( de.inum == 0 )  // unallocated dirent
		{
			continue;
		}

		// entry matches path element
		if ( namecmp( name, de.name ) == 0 )
		{
			if ( poff )
			{
				*poff = off;
			}

			inum = de.inum;

			return iget( dp->dev, inum );
		}
	}

	return 0;
}

// Write a new directory entry ( name, inum ) into the directory dp.
int dirlink ( struct inode* dp, char* name, uint inum )
{
	int           off;
	struct dirent de;
	struct inode* ip;

	// Check that name is not present.
	if ( ( ip = dirlookup( dp, name, 0 ) ) != 0 )
	{
		iput( ip );

		return - 1;
	}

	// Look for an empty dirent.
	for ( off = 0; off < dp->size; off += sizeof( de ) )
	{
		if ( readi( dp, ( char* ) &de, off, sizeof( de ) ) != sizeof( de ) )
		{
			panic( "dirlink: read" );
		}

		if ( de.inum == 0 )
		{
			break;
		}
	}

	// Write the new dirent
	strncpy( de.name, name, DIRNAMESZ );

	de.inum = inum;

	if ( writei( dp, ( char* ) &de, off, sizeof( de ) ) != sizeof( de ) )
	{
		panic( "dirlink" );
	}

	return 0;
}


// =================================================================================

// Paths

// Copy the next path element from path into name.
// Return a pointer to the element following the copied one.
// The returned path has no leading slashes,
// so the caller can check *path=='\0' to see if the name is the last one.
// If no name to remove, return 0.
//
// Examples:
//   skipelem( "a/bb/c", name ) = "bb/c", setting name = "a"
//   skipelem( "///a//bb", name ) = "bb", setting name = "a"
//   skipelem( "a", name ) = "", setting name = "a"
//   skipelem( "", name ) = skipelem( "////", name ) = 0
//
// JK:
//                        name | returned path
//                       ------|--------------
//   skipelem( "a/b/c" )  a    | "b/c" | 900
//   skipelem( "b/c" )    b    | "c"   | 901
//   skipelem( "c" )      c    | ""    | 902
//   skipelem( "" )       NA   | 0     | 0
static char* skipelem ( char* path, char* name )
{
	char* s;
	int   len;

	while ( *path == '/' )
	{
		path += 1;
	}

	if ( *path == '\0' )  // EOS...
	{
		return 0;
	}

	// Get element
	s = path;

	while ( *path != '/' && *path != 0 )
	{
		path += 1;
	}

	len = path - s;

	if ( len >= DIRNAMESZ )
	{
		memmove( name, s, DIRNAMESZ );
	}
	else
	{
		memmove( name, s, len );

		name[ len ] = 0;
	}

	// Get remaining path
	while ( *path == '/' )
	{
		path += 1;
	}

	return path;
}

// Look up and return the inode for a path name.
// If parent != 0, return the inode for the parent and copy the final
// path element into name, which must have room for DIRNAMESZ bytes.
// Must be called inside a transaction since it calls iput().
static struct inode* namex ( char* path, int nameiparent, char* name )
{
	struct inode *ip,
	             *next;

	// Choose starting point
	if ( *path == '/' )
	{
		ip = iget( ROOTDEV, ROOTINUM );
	}
	else
	{
		ip = idup( myproc()->cwd );
	}

	//
	while ( ( path = skipelem( path, name ) ) != 0 )  // If this is not the last path element
	{
		ilock( ip );

		// If current inode (parent) is not a directory, fail
		if ( ip->type != T_DIR )
		{
			iunlockput( ip );

			return 0;
		}

		// If nameiparent and retrieved last element (child),
		// return the current inode (parent)
		if ( nameiparent && *path == '\0' )
		{
			// Stop one level early.
			iunlock( ip );

			return ip;
		}

		// Get inode of next directory (child) in path
		next = dirlookup( ip, name, 0 );

		if ( next == 0 )
		{
			iunlockput( ip );

			return 0;
		}

		iunlockput( ip );

		ip = next;
	}

	// If consumed entire path before identiying parent, fail
	if ( nameiparent )
	{
		iput( ip );

		return 0;
	}

	return ip;
}

struct inode* namei ( char* path )
{
	char name [ DIRNAMESZ ];

	return namex( path, 0, name );
}

struct inode* nameiparent( char* path, char* name )
{
	return namex( path, 1, name );
}
