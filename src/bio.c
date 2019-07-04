// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents. Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.
//
// The implementation uses two state flags internally:
// * B_VALID : the buffer data has been read from the disk.
// * B_DIRTY : the buffer data has been modified
//             and needs to be written to disk.

/* The buffer cache has two jobs:
     . Synchronize access to disk blocks, to ensure that only one copy
       of a block is in memory and that only one kernel thread at a time
       uses that copy...
     . Cache popular blocks so that they don't need to be re-read from
       the slow disk

   Interface
     . bread  : Obtains a buf containing an in-memory copy of a disk block
     . bwrite : Writes a modified buffer to the appropriate block on the disk
     . brelse : Releases a buffer

   The buffer cache uses a per-buffer sleeplock to ensure that only one
   thread at a time uses each buffer
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

struct {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;

	// Array of buffers
	struct buf      buf [ NBUF ];

	/* Doubly-linked list of all buffers, through buf->prev and buf->next.
	    head->next is most recently released,
	    head->prev is least recently released.
	*/
	struct buf      head;

} bcache;

/* Initialize the linked list.
   All other accesses to the cache will use the linked list instead
   of the array.
*/
void binit ( void )
{
	struct buf* b;

	initlock( &bcache.lock, "bcache" );

	// Create linked list of buffers
	bcache.head.prev = &bcache.head;
	bcache.head.next = &bcache.head;

	for ( b = bcache.buf; b < bcache.buf + NBUF; b += 1 )
	{
		/* Insert at beginning of list...
		   | head | new_b | old_b | ... | tail |
		*/
		// 1) set the new element's attributes
		b->next = bcache.head.next;
		b->prev = &bcache.head;

		// 2) update right side element (old_b)
		bcache.head.next->prev = b;

		// 3) update left side element (head)
		bcache.head.next = b;


		initsleeplock( &b->lock, "buffer" );  // can concat idx to make name unique
	}
}

/* Look through buffer cache for block on device dev.
   If not found, allocate a buffer.
   In either case, return locked buffer.
*/
/* The buffer cache has a fixed number of buffers. If the fs
   asks for a block that is not already in the cache, a buffer
   currently holding some other data must be recycled.
   The bc recycles the least recently used (LRU) buffer
*/
static struct buf* bget ( uint dev, uint blockno )
{
	struct buf* b;

	/* Lock is held to ensure that there is at most one cached
	   buffer per disk block. Holding the lock causes the check
	   for (and designation if not present) to be an atomic operation.
	*/
	acquire( &bcache.lock );

	// Is the block already cached?
	/* Scan the buffer list for a buffer with the given device
	   and block numbers.
	   If we find one, acquire its sleeplock and return the
	   locked buffer.
	*/
	/* Most recently "used" elements are at the front of the list,
	   so we start our search at front towards back, with hope that
	   get lucky early and don't have to scan entire list.
	*/
	for ( b = bcache.head.next; b != &bcache.head; b = b->next )
	{
		if ( b->dev == dev && b->blockno == blockno )
		{
			b->refcnt += 1;

			release( &bcache.lock );

			/* It is safe to acquire the buffer's sleeplock outside
			   bcache.lock's critical section becuase the non-zero
			   value of b->refcnt prevents the buffer from being
			   reused for a different disk block...
			*/
			acquiresleep( &b->lock );

			return b;
		}
	}

	// Not cached; recycle an unused buffer.
	/* If there is no cached buffer for the given dev + blockno,
	   we must make one, possibly reusing a buffer.
	   Scan the buffer list a second time, looking for a buffer
	   that is not locked or dirty.

	   Even if refcnt==0, B_DIRTY indicates a buffer is in use
	   because log.c has modified it but not yet committed it.
	*/
	/* Least recently "used" elements are at the back of the list,
	   so we start our search at back towards front, with hope that
	   the block associated with the buffer we recycle isn't one
	   likely to be used soon.
	*/
	for ( b = bcache.head.prev; b != &bcache.head; b = b->prev )
	{
		if ( b->refcnt == 0 && ( b->flags & B_DIRTY ) == 0 )
		{
			// Update the buffer's metadata accordingly
			b->dev     = dev;
			b->blockno = blockno;
			b->refcnt  = 1;
			b->flags   = 0;  // Clear all flags including B_VALID
			                 // This ensures that bread will read the block
			                 // from disk instead of using old contents

			release( &bcache.lock );

			acquiresleep( &b->lock );

			return b;
		}
	}

	// If all the buffers are busy, panic.
	panic( "bget: no buffers" );
}

// Return a locked buf with the contents of the indicated block.
/* Obtains a buf containing an in-memory copy of a disk block.
*/
struct buf* bread ( uint dev, uint blockno )
{
	struct buf* b;

	b = bget( dev, blockno );

	// If the buffer needs to be read from disk, do so
	if ( ( b->flags & B_VALID ) == 0 )
	{
		iderw( b );
	}

	return b;
}

// Write b's contents to disk. Must be locked.
/* Writes a modified buffer to the appropriate block on the disk
*/
void bwrite ( struct buf* b )
{
	if ( ! holdingsleep( &b->lock ) )
	{
		panic( "bwrite" );
	}

	b->flags |= B_DIRTY;  // tell iderw to write (rather than read)

	iderw( b );
}

/* Release a locked buffer:
     . refcnt -= 1
     . release sleeplock
   If the refcnt reaches zero, move the released buffer to the
   front of the linked list.

   Moving the unused buffer to the front of the list causes the
   list to be ordered by how recently buffers were ?
   Because the buffer's metadata is not changed (dev + blockno),
   scanning the list can benefit from locality...
   Elements at the front of the list are the most recently used,
   and if locality of reference occurs, will likely be used again soon.
*/
void brelse ( struct buf* b )
{
	if ( ! holdingsleep( &b->lock ) )
	{
		panic( "brelse" );
	}

	releasesleep( &b->lock );

	acquire( &bcache.lock );

	b->refcnt -= 1;

	// No one is waiting for it.
	// Move the buffer to the beginning of the list...
	if ( b->refcnt == 0 )
	{
		// 1) update element on right side of the buf's old location
		b->next->prev = b->prev;

		// 2) update element on left side of the buf's old location
		b->prev->next = b->next;

		// 3) move buf to front of list
		b->next = bcache.head.next;
		b->prev = &bcache.head;

		// 4) update element on right side of the buf's new location
		bcache.head.next->prev = b;

		// 5) update element on left side of the buf's new location
		bcache.head.next = b;
	}

	release( &bcache.lock );
}
