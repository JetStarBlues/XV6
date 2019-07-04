struct buf {

	int               flags;           // valid, dirty
	uint              dev;             // device number ??
	uint              blockno;         // block number

	/* The per-buffer sleeplock is used to ensure that only
	   one thread/processes at a time uses each buffer.
	   That is, it serializes what would otherwise be
	   concurrent access.
	*/
	struct sleeplock  lock;

	uint              refcnt;              // ?

	struct buf       *prev;                // buffer cache linked list
	struct buf       *next;                // buffer cache linked list

	struct buf       *qnext;               // disk queue

	uchar             data [ BLOCKSIZE ];  // in-memory copy of disk contents
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
