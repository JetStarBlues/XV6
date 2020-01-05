/*
Physical memory allocator, intended to allocate memory for:
	. user processes
	. kernel stacks
	. page table pages
	. pipe buffers

Allocates 4096-byte pages.

Uses the physical memory between the end-of-the-kernel
and PHYSTOP for allocation.
*/

/*
Linked list of free pages.
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"


extern char* end;  /* first address after kernel text and static data.
                      Label is created by "kernel.ld" when creating the
                      kernel ELF */

struct node
{
	struct node* next;
};

struct _kmem
{
	struct spinlock lock;      // Held when modifying freelist
	int             use_lock;

	struct node* freelist;  // Where is this initialized ?? Is it default 0?
};

static struct _kmem kmem;


void freerange ( void* vstart, void* vend );


/* Initialization happens in two phases.
   1. main() calls kinit1() while still using entrypgdir to place just
   the pages mapped by entrypgdir on free list.
   2. main() calls kinit2() with the rest of the physical pages
   after installing a full page table that maps them on all cores.
*/

/* The reason for two calls, is that for much of main, one cannot use
   locks or memory above 4 MB?
*/
void kinit1 ( void* vstart, void* vend )
{
	initlock( &kmem.lock, "kmem" );

	kmem.use_lock = 0;

	freerange( vstart, vend );
}

void kinit2 ( void* vstart, void* vend )
{
	freerange( vstart, vend );

	kmem.use_lock = 1;
}

// Add memory to freelist via per-page calls to kfree
void freerange ( void* vstart, void* vend )
{
	char* p;

	p = ( char* ) PGROUNDUP( ( uint ) vstart );  // page align

	while ( p + PGSIZE <= ( char* ) vend )
	{
		kfree( p );

		p += PGSIZE;
	}
}

// Free the page of physical memory pointed at by vAddr,
// which normally should have been returned by a
// call to kalloc(). (The exception is when
// initializing the allocator; see kinit above.)
void kfree ( char* vAddr )
{
	struct node* np;

	// Not page aligned or outside valid range
	if ( ( uint ) vAddr % PGSIZE   ||
		 vAddr < end               ||  // kernel.end
		 V2P( vAddr ) >= PHYSTOP )
	{
		panic( "kfree" );
	}


	// Fill with junk to hopefully catch dangling refs.
	memset( vAddr, 1, PGSIZE );


	if ( kmem.use_lock )
	{
		acquire( &kmem.lock );
	}


	// Add the page to the start of the freelist
	np = ( struct node* ) vAddr;  // The page's "struct node" (pointer to the next free
	                              // page) is stored in the first bytes of the page

	np->next = kmem.freelist;  // record old start of the list in np->next

	kmem.freelist = np;        // set new start of list as np


	if ( kmem.use_lock )
	{
		release( &kmem.lock );
	}
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
char* kalloc ( void )
{
	struct node* np;

	if ( kmem.use_lock )
	{
		acquire( &kmem.lock );
	}


	// Remove and return first free element of list
	np = kmem.freelist;

	/* If not at end of freelist, update the list's head
	   to point to next free page
	*/
	if ( np )
	{
		kmem.freelist = np->next;
	}


	if ( kmem.use_lock )
	{
		release( &kmem.lock );
	}

	return ( char* ) np;  // np can be null...
}
