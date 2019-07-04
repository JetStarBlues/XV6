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

#define NDIRECT   12
#define NINDIRECT ( BLOCKSIZE / sizeof( uint ) )
#define MAXFILE   ( NDIRECT + NINDIRECT )

// On-disk inode structure
struct dinode
{
	short type;                   // File type
	short major;                  // Major device number (T_DEV only)
	short minor;                  // Minor device number (T_DEV only)
	short nlink;                  // Number of links to inode in file system
	uint  size;                   // Size of file (bytes)
	uint  addrs [ NDIRECT + 1 ];  // Data block addresses
};

// Inodes per block.
#define INODES_PER_BLOCK ( BLOCKSIZE / sizeof( struct dinode ) )

// Block containing inode i
#define IBLOCK( i, sb ) ( ( i ) / INODES_PER_BLOCK + sb.inodestart )

// Bitmap bits per block
#define BITS_PER_BLOCK ( BLOCKSIZE * 8 )

// Block of free map containing bit for block b
// JK - Because FSSIZE is 1000 blocks and BITS_PER_BLOCK is 4096,
//      this will always evaluate to 0 + sb.bmapstart
//      i.e. one bitmap block
#define BBLOCK( b, sb ) ( b / BITS_PER_BLOCK + sb.bmapstart )

// Directory is a file containing a sequence of dirent structures.
#define DIRNAMESZ 14

// Directory entry
struct dirent
{
	ushort inum;  // inode number
	char   name [ DIRNAMESZ ];
};
