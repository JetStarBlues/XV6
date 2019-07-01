struct file
{
	enum {

		FD_NONE,
		FD_PIPE,
		FD_INODE

	} type;

	int           ref;       // reference count
	char          readable;
	char          writable;
	struct pipe  *pipe;
	struct inode *ip;
	uint          off;
};


// in-memory copy of an inode
struct inode
{
	uint             dev;    // Device number
	uint             inum;   // Inode number
	int              ref;    // Reference count

	/* This lock is used for ... ??
	*/
	struct sleeplock lock;   // protects everything below here ??

	int              valid;  // inode has been read from disk?

	// copy of disk inode
	short            type;                   // File type
	short            major;                  // Major device number (T_DEV only)
	short            minor;                  // Minor device number (T_DEV only)
	short            nlink;                  // Number of links to inode in file system
	uint             size;                   // Size of file (bytes)
	uint             addrs [ NDIRECT + 1 ];  // Data block addresses
};

// table mapping major device number to
// device functions
struct devsw
{
	int ( *read  )( struct inode*, char*, int );
	int ( *write )( struct inode*, char*, int );
};

extern struct devsw devsw [];

#define CONSOLE 1
