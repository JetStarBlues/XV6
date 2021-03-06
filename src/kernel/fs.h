// On-disk file system format.
// Both the kernel and user programs use this header file.

#define ROOTINUM   1    // root i-number
#define BLOCKSIZE  512  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks | free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system.
// The super block describes the disk layout:
struct superblock
{
	uint size;         // Size of file system image (blocks)
	uint ninodes;      // Number of inodes.
	uint nlogblocks;   // Number of log blocks
	uint ndatablocks;  // Number of data blocks
	uint logstart;     // Block number of first log block
	uint inodestart;   // Block number of first inode block
	uint bmapstart;    // Block number of first free map block
};

/* TODO: A downside of having a large NDIRECT value is that most files
   are small, so this space is wasted per inode.
   (Typically NDIRECT is 12)
   A better solution for supporting large file sizes is to use
   multiple indirection. See:
    https://en.wikipedia.org/w/index.php?title=Inode_pointer_structure&oldid=912616146#Structure
*/
#define NDIRECT   18
#define NINDIRECT ( BLOCKSIZE / sizeof( uint ) )  // 128
#define MAXFILESZ ( NDIRECT + NINDIRECT )         // 146 blocks ( 74752 bytes)

// On-disk inode structure
struct dinode
{
	short          type;                   // File type
	short          major;                  // Major device number (T_DEV only)
	short          minor;                  // Minor device number (T_DEV only)
	short          nlink;                  // Number of links to inode in file system
	                                       /* I.e. number of directory entries that refer to
	                                          the on-disk inode */
	uint           size;                   // Size of file (bytes)
	uint           addrs [ NDIRECT + 1 ];  // Data block addresses 
	struct rtcdate mtime;                  /* Time of last modification
	                                          http://panda.moyix.net/~moyix/cs3224/fall16/hw7/hw7.html */


	/* "Because they need to fit neatly into one disk block,
	    the size of the 'struct dinode' must divide the
	    block size evenly"

	       sorted( factors( BLOCKSIZE ) )
	          [ 1, 2, 4, 8, 16, 32, 64, 128, 256, 512 ]

	       current size,
	          0.5 x 4  // type, major, minor
	          1        // size
	          18 + 1   // addrs
	          7        // mtime
	          ------
	          29
	          ------

	       needed padding,
	          32 - 29 = 3
	*/
	uint padding0;
	uint padding1;
	uint padding2;
};

// Inodes per block.
#define INODES_PER_BLOCK ( BLOCKSIZE / sizeof( struct dinode ) )

// Block containing inode i
#define IBLOCK( inum, sb ) ( ( inum ) / INODES_PER_BLOCK + sb.inodestart )

// Bitmap bits per block
#define BITS_PER_BLOCK ( BLOCKSIZE * 8 )

// Block of free map containing bit for block b
// JK - Because FSSIZE is 1000 blocks and BITS_PER_BLOCK is 4096,
//      this will always evaluate to 0 + sb.bmapstart
//      i.e. one bitmap block
#define BBLOCK( bnum, sb ) ( ( bnum ) / BITS_PER_BLOCK + sb.bmapstart )

// Directory is a file containing a sequence of dirent structures.
#define FILENAMESZ 14

// Directory entry
struct dirent
{
	ushort inum;                 // inode number
	char   name [ FILENAMESZ ];  /* If the name is shorter than FILENAMESZ,
	                                it is terminated by a null byte */
};
