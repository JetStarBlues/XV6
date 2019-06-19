// Physical memory allocator, intended to allocate
// memory for user processes, kernel stacks, page table pages,
// and pipe buffers. Allocates 4096-byte pages.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "spinlock.h"

void freerange ( void* vstart, void* vend );

extern char end [];  // first address after kernel loaded from ELF file
                     // defined by the kernel linker script in kernel.ld

struct run
{
	struct run* next;
};

struct
{
	struct spinlock lock;
	int             use_lock;
	struct run*     freelist;  // Where is this initialized ?? Is it default 0?

} kmem;

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

	p = ( char* ) PGROUNDUP( ( uint ) vstart );

	for ( ; p + PGSIZE <= ( char* ) vend; p += PGSIZE )
	{
		kfree( p );
	}
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc(). (The exception is when
// initializing the allocator; see kinit above.)
void kfree ( char* v )
{
	struct run* r;

	// Not page aligned or outside valid range
	if ( ( uint ) v % PGSIZE || v < end || V2P( v ) >= PHYSTOP )
	{
		panic( "kfree" );
	}


	// Fill with junk to hopefully catch dangling refs.
	memset( v, 1, PGSIZE );


	if ( kmem.use_lock )
	{
		acquire( &kmem.lock );
	}


	// Add the page to the start of the freelist
	r = ( struct run* ) v;  // The page's "struct run" (pointer to the next free
	                        // page) is stored in the first bytes of the page

	r->next = kmem.freelist;  // record old start of the list in r->next

	kmem.freelist = r;  // set new start of list as r


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
	struct run* r;

	if ( kmem.use_lock )
	{
		acquire( &kmem.lock );
	}


	// Remove and return first free element of list
	r = kmem.freelist;

	// If not at end of freelist, update to point to next free page
	if ( r )
	{
		kmem.freelist = r->next;
	}


	if ( kmem.use_lock )
	{
		release( &kmem.lock );
	}

	return ( char* ) r;  // r can be null...
}
