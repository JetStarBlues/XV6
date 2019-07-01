// File system implementation.  Five layers:
//   + Blocks: allocator for raw disk blocks.
//   + Log: crash recovery for multi-step updates.
//   + Files: inode allocator, reading, writing, metadata.
//   + Directories: inode with special contents ( list of other inodes! )
//   + Names: paths like /usr/rtm/xv6/fs.c for convenient naming.
//
// This file contains the low-level file system manipulation
// routines.  The ( higher-level ) system call implementations
// are in sysfile.c.

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

// static void itrunc ( struct inode* );

// there should be one superblock per disk device, but we run with
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

// Zero a block.
static void bzero ( int dev, int bno )
{
	struct buf* bp;

	bp = bread( dev, bno );

	memset( bp->data, 0, BSIZE );

	log_write( bp );

	brelse( bp );
}


// ___________________________________________________________________________

// Blocks.

// Allocate a zeroed disk block.
static uint balloc ( uint dev )
{
	struct buf* bp;
	int         b,
	            bidx,
	            bitmask;

	bp = 0;

	for ( b = 0; b < sb.size; b += BPB )  // for every block in the fs
	{
		bp = bread( dev, BBLOCK( b, sb ) );  // get the corresponding bitmap block

		for ( bidx = 0; bidx < BPB && ( b + bidx < sb.size ); bidx += 1 )  // for every bit in the bitmap block
		{
			bitmask = 1 << ( bidx % 8 );

			if ( ( bp->data[ bidx / 8 ] & bitmask ) == 0 )  // Is block free?
			{
				bp->data[ bidx / 8 ] |= bitmask;  // Mark block in use.

				log_write( bp );

				brelse( bp );

				bzero( dev, b + bidx );  // zero the block

				return b + bidx;
			}
		}

		brelse( bp );
	}

	panic( "balloc: out of blocks" );
}

// Free a disk block.
static void bfree ( int dev, uint b )
{
	struct buf* bp;
	int         bidx,
	            bitmask;

	readsb( dev, &sb );

	bp = bread( dev, BBLOCK( b, sb ) );

	bidx = b % BPB;

	bitmask = 1 << ( bidx % 8 );

	if ( ( bp->data[ bidx / 8 ] & bitmask ) == 0 )
	{
		panic( "bfree: freeing free block" );
	}

	bp->data[ bidx / 8 ] &= ~ bitmask;  // Mark block as free

	log_write( bp );

	brelse( bp );
}

// ___________________________________________________________________________

// Inodes.
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
// * Allocation: an inode is allocated if its type ( on disk )
//   is non-zero. ialloc() allocates, and iput() frees if
//   the reference and link counts have fallen to zero.
//
// * Referencing in cache: an entry in the inode cache
//   is free if ip->ref is zero. Otherwise ip->ref tracks
//   the number of in-memory pointers to the entry ( open
//   files and current directories ). iget() finds or
//   creates a cache entry and increments its ref; iput()
//   decrements ref.
//
// * Valid: the information ( type, size, &c ) in an inode
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
// get a long-term reference to an inode ( as for an open file )
// and only lock it for short periods ( e.g., in read() ).
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

struct {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;

	struct inode    inode [ NINODE ];

} icache;

void iinit ( int dev )
{
	int i = 0;

	initlock( &icache.lock, "icache" );

	for ( i = 0; i < NINODE; i += 1 )
	{
		initsleeplock( &icache.inode[ i ].lock, "inode" );  // can concat idx to make name unique
	}

	readsb( dev, &sb );

	/*cprintf(

		"superblock:\n"
		"    size       %d\n"
		"    nblocks    %d\n"
		"    ninodes    %d\n"
		"    nlog       %d\n"
		"    logstart   %d\n"
		"    inodestart %d\n"
		"    bmapstart  %d\n\n",

		sb.size, sb.nblocks, sb.ninodes, sb.nlog,
		sb.logstart, sb.inodestart, sb.bmapstart
	);*/

	cprintf( "superblock:\n" );
	cprintf( "    size (total blocks) %d\n",   sb.size       );
	cprintf( "    nblocks(data)       %d\n",   sb.nblocks    );
	cprintf( "    ninodes             %d\n",   sb.ninodes    );
	cprintf( "    ninodeblocks        %d\n",   ( sb.ninodes / IPB ) + 1 );
	cprintf( "    nlog(blocks)        %d\n",   sb.nlog       );
	cprintf( "    logstart            %d\n",   sb.logstart   );
	cprintf( "    inodestart          %d\n",   sb.inodestart );
	cprintf( "    bmapstart           %d\n\n", sb.bmapstart  );
}

static struct inode* iget ( uint dev, uint inum );

// Allocate an inode on device dev.
// Mark it as allocated by giving it type type.
// Returns an unlocked but allocated and referenced inode.
struct inode* ialloc ( uint dev, short type )
{
	int            inum;
	struct buf*    bp;
	struct dinode* dip;

	for ( inum = 1; inum < sb.ninodes; inum += 1 )  // for each inode
	{
		bp = bread( dev, IBLOCK( inum, sb ) );

		dip = ( struct dinode* ) bp->data + ( inum % IPB );

		// a free inode
		if ( dip->type == 0 )
		{  
			memset( dip, 0, sizeof( *dip ) );  // clear contents

			dip->type = type;

			log_write( bp );   // mark it allocated on the disk

			brelse( bp );

			return iget( dev, inum );  // return in-memory copy
		}

		brelse( bp );
	}

	panic( "ialloc: no inodes" );
}

// Copy a modified in-memory inode to disk.
// Must be called after every change to an ip->xxx field
// that lives on disk, since i-node cache is write-through.
// Caller must hold ip->lock.
void iupdate ( struct inode* ip )
{
	struct buf*    bp;
	struct dinode* dip;

	bp = bread( ip->dev, IBLOCK( ip->inum, sb ) );

	dip = ( struct dinode* ) bp->data + ( ip->inum % IPB );

	dip->type  = ip->type;
	dip->major = ip->major;
	dip->minor = ip->minor;
	dip->nlink = ip->nlink;
	dip->size  = ip->size;

	memmove( dip->addrs, ip->addrs, sizeof( ip->addrs ) );

	log_write( bp );

	brelse( bp );
}

// Find the inode with number inum on device dev
// and return the in-memory copy. Does not lock
// the inode and does not read it from disk.
static struct inode* iget ( uint dev, uint inum )
{
	struct inode* ip;
	struct inode* empty;

	acquire( &icache.lock );

	empty = 0;

	// Is the inode already cached?
	for ( ip = &icache.inode[ 0 ]; ip < &icache.inode[ NINODE ]; ip += 1 )
	{
		if ( ip->ref > 0 && ip->dev == dev && ip->inum == inum )
		{
			ip->ref += 1;

			release( &icache.lock );

			return ip;
		}

		if ( empty == 0 && ip->ref == 0 )  // Remember empty slot.
		{
			empty = ip;
		}
	}

	// Recycle an inode cache entry.
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

// Increment reference count for ip.
// Returns ip to enable ip = idup( ip1 ) idiom.
struct inode* idup ( struct inode* ip )
{
	acquire( &icache.lock );

	ip->ref += 1;

	release( &icache.lock );

	return ip;
}

// Lock the given inode.
// Reads the inode from disk if necessary.
void ilock ( struct inode* ip )
{
	struct buf    *bp;
	struct dinode *dip;

	if ( ip == 0 || ip->ref < 1 )
	{
		panic( "ilock" );
	}

	acquiresleep( &ip->lock );

	if ( ip->valid == 0 )
	{
		bp = bread( ip->dev, IBLOCK( ip->inum, sb ) );

		dip = ( struct dinode* ) bp->data + ( ip->inum % IPB );

		ip->type  = dip->type;
		ip->major = dip->major;
		ip->minor = dip->minor;
		ip->nlink = dip->nlink;
		ip->size  = dip->size;

		memmove( ip->addrs, dip->addrs, sizeof( ip->addrs ) );

		brelse( bp );

		ip->valid = 1;

		if ( ip->type == 0 )
		{
			panic( "ilock: no type" );
		}
	}
}

// Unlock the given inode.
void iunlock ( struct inode* ip )
{
	if ( ip == 0 || ! holdingsleep( &ip->lock ) || ip->ref < 1 )
	{
		panic( "iunlock" );
	}

	releasesleep( &ip->lock );
}

// Drop a reference to an in-memory inode.
// If that was the last reference, the inode cache entry can be recycled.
// If that was the last reference and the inode has no links
// to it, free the inode (and its content) on disk.
// All calls to iput() must be inside a transaction in
// case it has to free the inode.
void iput ( struct inode* ip )
{
	acquiresleep( &ip->lock );

	if ( ip->valid && ip->nlink == 0 )
	{
		acquire( &icache.lock );

		int r = ip->ref;

		release( &icache.lock );

		// inode has no links and no other references: truncate and free.
		if ( r == 1 )
		{
			itrunc( ip );

			ip->type = 0;

			iupdate( ip );

			ip->valid = 0;
		}
	}

	releasesleep( &ip->lock );

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

// Inode content
//
// The content (data) associated with each inode is stored
// in blocks on the disk. The first NDIRECT block numbers
// are listed in ip->addrs[]. The next NINDIRECT blocks are
// listed in block ip->addrs[NDIRECT].

// Return the disk block address of the nth block in inode ip.
// If there is no such block, bmap allocates one.
static uint bmap ( struct inode* ip, uint bn )
{
	uint        addr;
	uint*       a;
	struct buf* bp;

	// Get from direct blocks
	if ( bn < NDIRECT )
	{
		if ( ( addr = ip->addrs[ bn ] ) == 0 )
		{
			addr = balloc( ip->dev );

			ip->addrs[ bn ] = addr;
		}

		return addr;
	}

	// Get from indirect block
	bn -= NDIRECT;

	if ( bn < NINDIRECT )
	{
		// Load indirect block, allocating if necessary.
		if ( ( addr = ip->addrs[ NDIRECT ] ) == 0 )
		{
			addr = balloc( ip->dev );

			ip->addrs[ NDIRECT ] = addr;
		}

		// Get block from indirect block, allocating if necessary.
		bp = bread( ip->dev, addr );

		a = ( uint* ) bp->data;

		if ( ( addr = a[ bn ] ) == 0 )
		{
			addr = balloc( ip->dev );

			a[ bn ] = addr;

			log_write( bp );  // ??
		}

		brelse( bp );

		return addr;
	}

	panic( "bmap: out of range" );
}

// Truncate inode (discard contents).
// Only called when the inode has no links
// to it (no directory entries referring to it)
// and has no in-memory reference to it (is
// not an open file or current directory).

// static void itrunc ( struct inode* ip )
void itrunc ( struct inode* ip )  // JK - make public so can use for O_TRUNC...
{
	int         i,
	            j;
	struct buf* bp;
	uint*       a;

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
		bp = bread( ip->dev, ip->addrs[ NDIRECT ] );

		a = ( uint* )bp->data;

		for ( j = 0; j < NINDIRECT; j += 1 )
		{
			if ( a[ j ] )
			{
				bfree( ip->dev, a[ j ] );
			}
		}

		brelse( bp );

		// Free the indirect block itself
		bfree( ip->dev, ip->addrs[ NDIRECT ] );

		ip->addrs[ NDIRECT ] = 0;
	}

	ip->size = 0;  // reset inode size

	iupdate( ip );
}

// Copy stat information from inode.
// Caller must hold ip->lock.
void stati ( struct inode* ip, struct stat* st )
{
	st->dev   = ip->dev;
	st->ino   = ip->inum;
	st->type  = ip->type;
	st->nlink = ip->nlink;
	st->size  = ip->size;
}

// Read data from inode.
// Caller must hold ip->lock.
int readi ( struct inode* ip, char* dst, uint off, uint n )
{
	uint        tot,
	            m;
	struct buf* bp;

	if ( ip->type == T_DEV )
	{
		if ( ip->major < 0 || ip->major >= NDEV || ! devsw[ ip->major ].read )
		{
			return - 1;
		}

		return devsw[ ip->major ].read( ip, dst, n );
	}

	// Read starts beyond EOF, or (n < 1)
	if ( off > ip->size || off + n < off )
	{
		return - 1;
	}

	// Read crosses EOF
	if ( off + n > ip->size )
	{
		n = ip->size - off;
	}

	// Copy data from inode data blocks to dst
	for ( tot = 0; tot < n; tot += m )
	{
		bp = bread( ip->dev, bmap( ip, off / BSIZE ) );

		m = min( n - tot, BSIZE - ( off % BSIZE ) );

		memmove( dst, bp->data + ( off % BSIZE ), m );

		brelse( bp );

		off += m;
		dst += m;
	}

	return n;
}

// Write data to inode.
// Caller must hold ip->lock.
int writei ( struct inode* ip, char* src, uint off, uint n )
{
	uint        tot,
	            m;
	struct buf* bp;

	if ( ip->type == T_DEV )
	{
		if ( ip->major < 0 || ip->major >= NDEV || ! devsw[ ip->major ].write )
		{
			return - 1;
		}

		return devsw[ ip->major ].write( ip, src, n );
	}

	// Write starts beyond EOF, or (n < 1)
	if ( off > ip->size || off + n < off )
	{
		return - 1;
	}

	// Write grows beyond maximum file size
	if ( off + n > MAXFILE * BSIZE )
	{
		return - 1;
	}

	// Copy data from src to inode data blocks
	for ( tot = 0; tot < n; tot += m )
	{
		bp = bread( ip->dev, bmap( ip, off / BSIZE ) );

		m = min( n - tot, BSIZE - ( off % BSIZE ) );

		memmove( bp->data + ( off % BSIZE ), src, m );

		log_write( bp );

		brelse( bp );

		off += m;
		src += m;
	}

	// Write crossed EOF, update file size
	if ( n > 0 && off > ip->size )
	{
		ip->size = off;

		iupdate( ip );
	}

	return n;
}


// ___________________________________________________________________________

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


// ___________________________________________________________________________

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
		ip = iget( ROOTDEV, ROOTINO );
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
