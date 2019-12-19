/* Code from:
     The C Programming Language, Kernighan & Ritchie,
     2nd ed. Section 8.7
*/

#include "types.h"
#include "stat.h"
#include "user.h"
#include "param.h"
#include "mmu.h"

/*
	. The space malloc manages may not be contiguous. Thus its
	  free storage is kept as a list of free blocks.
	. Each block contains a size, a pointer to the next block,
	  and the space itself.

	. When a request is made, the free list is scanned until a
	  big-enough block is found - "first fit"
	. If the block is exactly the size requested, it is unlinked
	  from the list and returned to the user.
	. If it is too big, it is split, and the proper amount is
	  returned to the user, while the residual remains in the
	  free list
	. If no big-enough block is found, another large chunk is
	  obtained from the OS and linked into the free list.

	. Freeing causes a search of the free list, to find the a
	  proper place to insert the block
	. If the block is adjacent to a free block on either side,
	  it is coalesced with it into a single bigger block
	. Determining adjacency is easy because the list is maintained
	  in order of increasing address

	. Something about alignment...
	. To simplify alignment, all blocks are multiples of the
	  header size, and the header is aligned properly...
	. This is achieved by a union that contains the desired
	  header structure and an instance of the most restrictive
	  alignment type.

	. The requested size in bytes is rounded up to the proper
	  number of header-sized units.
	. The block that will be allocated contains one more unit
	  for the header itself.
	. The pointer returned by malloc points at the free space,
	  not at the header itself.

	           --->      ->  -----------
	          |         |    nextptr
	          |  header |    -----------
	          |         |    size
	    block |          ->  -----------  <- pointer to free space
	          |         |
	          |    free |    ...
	          |   space |
	          |         |
	           --->      ->  -----------
*/

/* Most restrictive integer type with regards to alignment.
   Machine dependent. long is arbitrarily chosen here.
*/
typedef long Align;

union header {

	struct {

		union header* nextptr;  // pointer to next block if on free list
		uint          size;     // size of this block (header + freespace)

	} h;

	Align x;  // present to force alignment. Not used otherwise.
};

typedef union header Header;


/* According to stackoverflow.com/a/36512105,

   By having first block of free list as global,
   (and thus allocated space in the region where
   statics reside),
   can assume that it will never be contiguously adjacent
   in memory address to the other blocks in the free list
   which are dynamically allocated from the heap...

   This means it can never be merged, and thus
   at least one block will always be present in the free list
*/
static Header base;  // initial block of the free list

static Header* freelistPtr = NULL;  // pointer to a block in the free list


static int morecore ( uint );
void       free     ( void* );

/* Return ceiling of x/y integer division
   stackoverflow.com/a/503201
*/
static int ceilingDivide ( int x, int y )
{
	return ( ( x - 1 ) / y ) + 1;
}

/* See annotations at stackoverflow.com/a/36512105
*/
void* malloc ( uint nbytes )
{
	Header* blockPtr;
	Header* prevBlockPtr;
	uint    nunits;

	/* To simplify alignment, blocks are multiples of the
	   header size.
	   The requested size in bytes is rounded up to the
	   proper number of header-sized units...
	*/
	nunits = ceilingDivide( nbytes, sizeof( Header ) );

	nunits += 1;  // save room for block header


	// No free list yet, create one
	if ( freelistPtr == NULL )
	{
		/* The initial freelist consists of one block of
		   size zero, that points to itself.
		*/
		base.h.nextptr = &base;
		base.h.size    = 0;

		// Point to the block...
		freelistPtr = &base;
	}


	// Initialize pointers
	prevBlockPtr = freelistPtr;
	blockPtr     = freelistPtr->h.nextptr;


	// Search list for an appropriate block
	while ( 1 )
	{
		// Big enough
		if ( blockPtr->h.size >= nunits )
		{
			// Exact size
			if ( blockPtr->h.size == nunits )
			{
				// Remove block from free list
				prevBlockPtr->h.nextptr = blockPtr->h.nextptr;
			}

			// Bigger
			else
			{
				// Split into two chunks...

				/* Create chunk to be leftover.
				   It's size is currentSize - requestedSize
				*/
				blockPtr->h.size -= nunits;

				/* Create chunk to be returned.
				   It's size is requestedSize
				*/
				blockPtr += blockPtr->h.size;

				blockPtr->h.size = nunits;
			}

			/* Set global starting position, so that next time
			   called, we start searching from where we left off.
			   Aims to spread out?...
			*/
			freelistPtr = prevBlockPtr;

			// Return a pointer to the block's free space
			return ( void* ) ( blockPtr + 1 );
		}


		/* If we've wrapped around the free list without finding
		   a big enough block, request more memory from OS.
		*/
		if ( blockPtr == freelistPtr )
		{
			// Failed to get more memory
			if ( morecore( nunits ) < 0 )
			{
				return NULL;
			}

			// morecore might have changed freelistPtr
			blockPtr = freelistPtr;
		}


		// Examine next block in free list
		prevBlockPtr = blockPtr;
		blockPtr     = blockPtr->h.nextptr;
	}
}


static int morecore ( uint nunits )
{
	char*   blockPtr;
	Header* headerPtr;

	/* Request at least this much because asking the OS
	   for memory is comparatively slower, than dolling
	   it out from our free list...
	*/
	if ( nunits < PGSIZE )
	{
		nunits = PGSIZE;
	}

	//
	blockPtr = sbrk( nunits * sizeof( Header ) );

	// On error, sbrk returns ( char* ) ( - 1 )
	if ( blockPtr == ( char* ) ( - 1 ) )
	{
		return - 1;  // error, failed to get more memory
	}


	// 
	headerPtr = ( Header* ) blockPtr;

	headerPtr->h.size = nunits;


	/* Insert the block into the free list.
	   +1 because free() expects pointer to block's free space.
	*/
	free( ( void* ) ( headerPtr + 1 ) );


	//
	return 0;
}


/* Scans the free list, starting at freelistPtr, looking
   for a place to insert the free block.
*/
/* Note that when adding a block to the free list,
   'free' does not zero its contents.
   Thus blocks returned by malloc should be treated as
   containing garbage data.
*/
void free ( void* blockFreeSpacePtr )
{
	Header* blockPtr;
	Header* curBlockPtr;

	/* Get pointer to block's header from the
	   given pointer to its free space
	*/
	blockPtr = ( Header* ) blockFreeSpacePtr - 1;


	//
	curBlockPtr = freelistPtr;

	//
	while ( 1 )
	{
		// Address falls between current block and next block
		if ( ( blockPtr > curBlockPtr ) &&
			 ( blockPtr < curBlockPtr->h.nextptr ) )
		{
			break;
		}

		/* Address is higher than highest address in free list,
		   or address is lower than lowest address in free list
		*/
		if ( ( curBlockPtr >= curBlockPtr->h.nextptr ) &&  // reached rightmost block
			 (
			   ( blockPtr > curBlockPtr ) ||          // block is higher than rightmost
			   ( blockPtr < curBlockPtr->h.nextptr )  // block is lower than leftmost
			 )
		   )
		{
			break;
		}

		// Evaluate next block in free list
		curBlockPtr = curBlockPtr->h.nextptr;
	}


	// If next adjacent contiguous block is also in the free list, merge
	if ( blockPtr + blockPtr->h.size == curBlockPtr->h.nextptr )
	{
		// Absorb adjacent block
		blockPtr->h.size += curBlockPtr->h.nextptr->h.size;

		// Update next pointer (to that of absorbed block)
		blockPtr->h.nextptr = curBlockPtr->h.nextptr->h.nextptr;
	}
	else
	{
		// Update next pointer of block
		blockPtr->h.nextptr = curBlockPtr->h.nextptr;
	}


	// If previous adjacent contiguous block is also in the free list, merge
	if ( curBlockPtr + curBlockPtr->h.size == blockPtr )
	{
		// Absorb adjacent block
		curBlockPtr->h.size += blockPtr->h.size;

		// Update next pointer (to that of absorbed block)
		curBlockPtr->h.nextptr = blockPtr->h.nextptr;
	}
	else
	{
		// Update next pointer of previous block
		curBlockPtr->h.nextptr = blockPtr;
	}


	/* Point to block before the one we have just inserted,
	   so that malloc starts its search here.
	*/
	freelistPtr = curBlockPtr;
}


// ___________________________________________________________________________________

/* Crude implementation of realloc based on man page description.
*/

void* realloc ( void* oldPtr, uint newSize )
{
	Header* blockPtr;
	void*   newPtr;
	uint    oldSize;
	uint    nBytesToCopy;

	// If oldPtr is NULL, the call is equivalent to malloc
	if ( oldPtr == NULL )
	{
		oldPtr = malloc( newSize );

		return oldPtr;
	}

	/* If oldPtr is not NULL, and newSize is zero, the
	   call is equivalent to free.
	*/
	else if ( newSize == 0 )
	{
		free( oldPtr );

		oldPtr = NULL;

		return NULL;
	}


	// Allocate new block of size newSize
	newPtr = malloc( newSize );

	// If failed to allocate new block, return the old block untouched
	if ( newPtr == NULL )
	{
		return oldPtr;
	}


	// Get pointer to old block's header
	blockPtr = ( Header* ) oldPtr - 1;

	// Get size of old block
	oldSize = blockPtr->h.size;


	/* Compare apples to apples.
	   Convert newSize from bytes to same units used by oldSize.
	*/
	newSize = ceilingDivide( newSize, sizeof( Header ) );

	newSize += 1;  // save room for block header


	// Copy old contents (up to minimum of old size and new size)
	nBytesToCopy = newSize < oldSize ? newSize : oldSize;
	nBytesToCopy -= 1;                 // remove size of block's header
	nBytesToCopy *= sizeof( Header );  // convert units to bytes

	memcpy( newPtr, oldPtr, nBytesToCopy );


	// Free old block
	free( oldPtr );

	// Set oldPtr to point to new block
	oldPtr = newPtr;

	//
	return oldPtr;
}


// ___________________________________________________________________________________

/* TODO: function to print visualization of blocks currently managing
*/
