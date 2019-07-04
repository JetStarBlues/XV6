#define NPROC           64                   // max number of processes
#define KSTACKSIZE      4096                 // size of per-process kernel stack
#define NCPU            8                    // max number of CPUs
#define NOPENFILE_PROC  16                   // max number of open files per process
#define NOPENFILE_SYS   100                  // max number of open files per system ??
#define NINODE          50                   // max number of active i-nodes
#define NDEV            10                   // max major device number
#define ROOTDEV         1                    // device number of file system root disk
#define MAXARG          32                   // max exec arguments
#define MAXOPBLOCKS     10                   // max number of blocks any FS op writes
#define LOGSIZE         ( MAXOPBLOCKS * 3 )  // max data blocks in on-disk log
#define NBUF            ( MAXOPBLOCKS * 3 )  // size of disk block cache
#define FSSIZE          1000                 // size of file system in blocks
