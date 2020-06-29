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
	struct pipe*  pipe;
	struct inode* ip;
	uint          offset;    // Read/write offset...
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
	short            type;                   // Inode type (T_DIR, T_FILE, T_DEV)
	short            major;                  // Major device number (T_DEV only)
	short            minor;                  // Minor device number (T_DEV only)
	short            nlink;                  // Number of links to inode in file system.
	                                         /* I.e. number of directory entries that refer to
	                                            the on-disk inode */
	uint             size;                   // Size of file (bytes)
	uint             addrs [ NDIRECT + 1 ];  // Data block addresses
	struct rtcdate   mtime;                  // Time of last modification
};


// Table mapping major device numberd to their respective
// read and write functions
struct devsw
{
	int ( *read  )( struct inode*, char*, int );  // ( struct inode* ip, char* dst, int n )
	int ( *write )( struct inode*, char*, int );  // ( struct inode* ip, char* src, int n )
	int ( *ioctl )( struct inode*, int, ... );    // ( struct inode* ip, int request, ... )
};

extern struct devsw devsw [];

/* Major device numbers */
// 0 - unused?
// 1 - ROOTDEV
#define CONSOLE 2
#define DISPLAY 3
// DEVNULL  // (minor0: null, minor1: zero)
// MOUSE
// KEYBOARD
// DEVRANDOM  // stackoverflow.com/a/19987142

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


// Lseek defines
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
