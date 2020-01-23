// Inode types
#define T_DIR  1  // Directory
#define T_FILE 2  // File
#define T_DEV  3  // Device

struct stat
{
	int  dev;              // Device number
	uint ino;              // Inode number

	short          type;   // Inode type
	short          nlink;  // Number of links to file
	uint           size;   // Size of file in bytes
	struct rtcdate mtime;  // Time of last modification
};
