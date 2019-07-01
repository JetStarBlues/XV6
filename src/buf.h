struct buf {

	int               flags;           // ?
	uint              dev;             // device number ??
	uint              blockno;         // block number

	/* This lock is used for ... ??
	*/
	struct sleeplock  lock;

	uint              refcnt;          // ?

	// ...
	struct buf       *prev;            // LRU cache list
	struct buf       *next;            // MRU cache list

	// ...
	struct buf       *qnext;           // disk queue

	// ...
	uchar             data [ BSIZE ];  // in-memory copy of disk contents
};

#define B_VALID 0x2  // buffer has been read from disk
#define B_DIRTY 0x4  // buffer needs to be written to disk


/* xv6's block size is identical to the disk's sector size (512 bytes).
   To represent a block, xv6 uses a "struct buf".
   The data in a "buf" is often out of sync with the disk:
     . not yet read in from disk, or
     . if updated, not yet written out to disk

   The driver code (ide.c) can support any block size that is a
   multiple of the sector size ??

   The flags B_VALID and B_DIRTY track the relationship between memory
   and disk. 
*/
