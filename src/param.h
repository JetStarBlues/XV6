#define NCPU            8                    // max number of CPUs

#define KSTACKSIZE      4096                 // size of per-process kernel stack

#define NPROC           64                   // max number of processes

#define NOPENFILE_PROC  16                   // max number of open files per process
#define NOPENFILE_SYS   100                  // max number of open files per system ??
#define NINODE          50                   // max number of active inodes

#define NDEV            10                   // max major device number
#define ROOTDEV         1                    // device number of file system root disk ??

#define MAXARG          32                   // max exec arguments

#define MAXOPBLOCKS     10                   // max number of blocks an FS syscall can write at once
#define LOGSIZE         ( MAXOPBLOCKS * 3 )  // number of blocks in the log
#define NBUF            ( MAXOPBLOCKS * 3 )  // number of buffers in the buffer cache

#define FSSIZE          4000                 // size of file system in blocks
#define FSNINODE        200                  // number of inodes in file system
