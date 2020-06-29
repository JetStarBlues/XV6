#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x040  // create file if it does not exist
#define O_TRUNC   0x200  // truncate/overwrite (file is truncated to length 0)
#define O_APPEND  0x400  // append (file offset is positioned at end of file)

// see man 2 open
