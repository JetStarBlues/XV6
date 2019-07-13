// An open file
struct file
{
	enum {

		FD_NONE,
		FD_PIPE,
		FD_INODE

	} type;

	int           ref;       // Reference count - number of references to the file
	char          readable;  // File opened for reading
	char          writable;  // File opened for writing
	struct pipe  *pipe;
	struct inode *ip;
	uint          off;       // IO offset...
};


// In-memory copy of an inode
struct inode
{
	uint             dev;    // Device number
	uint             inum;   // Inode number
	int              ref;    // Reference count - number of C pointers referring to
	                         // this in-memory copy

	/* This lock is used for ... ??
	*/
	struct sleeplock lock;   // protects everything below here ??

	int              valid;  // inode has been read from disk?

	// Copy of disk inode
	short            type;                   // File type
	short            major;                  // Major device number (T_DEV only)
	short            minor;                  // Minor device number (T_DEV only)
	short            nlink;                  // Number of links to inode in file system.
	                                         /* I.e. number of directory entries that refer to
	                                            the on-disk inode */
	uint             size;                   // Size of file (bytes)
	uint             addrs [ NDIRECT + 1 ];  // Data block addresses
};

// Table mapping major device numberd to their respective
// read and write functions
struct devsw
{
	int ( *read  )( struct inode*, char*, int );
	int ( *write )( struct inode*, char*, int );
};

extern struct devsw devsw [];

// Major device numbers
#define CONSOLE 1
#define DISPLAY 2  // hmm...
// MOUSE
// KEYBOARD

/* Major vs minor device number
     . https://www.oreilly.com/library/view/linux-device-drivers/0596000081/ch03s02.html
     . The major number identifies the driver associated with
       the device. For example, /dev/null and /dev/zero are both
       managed by the same driver.
     . The minor number is used only by the driver. Other parts
       of the kernel don’t use it, and merely pass it along to
       the driver. It is common for a driver to control several
       devices. The minor number provides a way for the driver
       to differentiate among them. 
*/
