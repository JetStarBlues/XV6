/*
Startup process:
	. BIOS on motherboard prepares CPU's hardware

	. The BIOS then loads the first 512 byte sector
	  from the "boot sector" into memory

	. xv6's boot sector is the first 512 byte sector
	  in "xv6.img". This corresponds to the binary
	  "bootblock" generated from "bootasm.S" and "bootmain.c".
	  See Makefile.

	. The code in bootblock sets up the CPU to run xv6 by
	  among other things:
	    . switching to 32bit mode
	    . loading the "kernel" binary into memory.
	      The "kernel" is the remaining sectors in "xv6.img"
	      (see Makefile).
	    . jumping to the "kernel" ELF's entry point, which
	      corresponds to label "_start" in "entry.S"

	. entry.S among other things:
		. turns on paging
		. sets up the (temporary) stack pointer ??
		. calls "main"

	. "main" initializes a bunch of OS things.
	  Eventually it calls "userinit" which loads the
	  "initcode" binary into the first process

	. The code in initcode calls sys_exec( fs/bin/init )

	. The code in fs/bin/init starts the shell
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

static void startothers ( void );
static void mpmain      ( void )  __attribute__( ( noreturn ) );

extern pde_t* kpgdir;

extern char end [];  /* first address after kernel text and static data.
                        Label is created by "kernel.ld" when creating the
                        kernel ELF */

// Bootstrap CPU starts running C code here.
// Allocate a real stack and switch to it, first
// doing some setup required for memory allocator to work.
int main ( void )
{
	kinit1( end, P2V( 4 * 1024 * 1024 ) );  // kernel_end..4MB  ?? phys page allocator

	kvmalloc();      // create kernel page table
	mpinit();        // detect other CPUs
	lapicinit();     // interrupt controller
	seginit();       // segment descriptors
	picinit();       // disable pic
	ioapicinit();    // another interrupt controller
	consoleinit();   // console hardware
	uartinit();      // serial port
	mouseinit();     // mouse
	vgainit();       // vga
	displayinit();   // generic display
	pinit();         // process table
	tvinit();        // trap vectors
	binit();         // buffer cache
	fileinit();      // file table
	ideinit();       // disk 
	startothers();   // start other CPUs

	kinit2( P2V( 4 * 1024 * 1024 ), P2V( PHYSTOP ) );  // 4MB..PHYSTOP  ?? must come after startothers()

	userinit();      // create first user process
	mpmain();        // finish this CPU's setup

	/* The following are initialized by the first process:

	   iinit( ROOTDEV );    // ...
	   initlog( ROOTDEV );  // initialize log. Recover file system if necessary
	*/
}

// Other CPUs jump here from entryother.S.
static void mpenter ( void )
{
	switchkvm();
	seginit();
	lapicinit();
	mpmain();
}

// Common CPU setup code.
static void mpmain ( void )
{
	cprintf( "cpu%d: starting %d\n\n", cpuid(), cpuid() );

	idtinit();  // load idt register

	xchg( &( mycpu()->started ), 1 );  // tell startothers() we're up

	scheduler();  // start running processes
}

pde_t entrypgdir [];  // For entry.S

// Start the non-boot (AP) processors.
static void startothers ( void )
{
	// extern uchar _binary_entryother_start[], _binary_entryother_size[];
	extern uchar _binary_img_entryother_start [],
	             _binary_img_entryother_size  [];  // JK, new path

	uchar*      code;
	struct cpu* c;
	char*       stack;

	// Write entry code to unused memory at 0x7000.
	// The linker has placed the image of entryother.S in
	// _binary_entryother_start.
	code = P2V( 0x7000 );

	memmove(

		code,
		// _binary_entryother_start,
		// ( uint ) _binary_entryother_size
		_binary_img_entryother_start,
		( uint ) _binary_img_entryother_size
	);

	for ( c = cpus; c < cpus + ncpu; c += 1 )
	{
		if ( c == mycpu() )  // We've started already.
		{
			continue;
		}

		/* Tell entryother.S,
		     . what stack to use,
		     . where to enter,
		     . what pgdir to use

		   We cannot use kpgdir yet, because the AP processor
		   is running in low memory, so we use entrypgdir for the APs too.
		*/
		/* TODO: What does this do??

		    0x7000 + _binary_entryother_size ->  -------------------------
		                                         contents of entryother.S
		    0x7000                           ->  -------------------------  <- start
		                                         address of ...
		    0x7000 - 4                       ->  -------------------------  <- start - 4
		                                         address of mpenter
		    0x7000 - 8                       ->  -------------------------  <- start - 8
		                                         address of ...
		    0x7000 - 12                      ->  -------------------------  <- start - 12


		   See stackoverflow.com/a/57174081

		                  ( void** ) ( code - 4 )  -> pointer to pointer to void
		    ( void ( ** ) ( void ) ) ( code - 8 )  -> pointer to pointer to function (with no return value, no parameters)
		                   ( int** ) ( code - 12 ) -> pointer to pointer to int
		*/
		stack = kalloc();

		*(                ( void** ) ( code - 4  )  ) = stack + KSTACKSIZE;  // stack top since it grows downwards ?

		*(  ( void ( ** ) ( void ) ) ( code - 8  )  ) = mpenter;

		*(                 ( int** ) ( code - 12 )  ) = ( void* ) V2P( entrypgdir );  // pdgdir to use

		lapicstartap( c->apicid, V2P( code ) );


		// Wait for cpu to finish mpmain()
		while ( c->started == 0 )
		{
			//
		}
	}
}

// The boot page table used in entry.S and entryother.S.
// Page directories (and page tables) must start on page boundaries,
// hence the __aligned__ attribute.
// PTE_PS in a page directory entry enables 4Mbyte "super" pages.
__attribute__( ( __aligned__( PGSIZE ) ) )

// See p.22 and p.37 for explanation...
pde_t entrypgdir [ NPDENTRIES ] = {

	/* Map VA's [0, 4MB] to PA's [0, 4MB]...

	   1:1 mapping

	   This mapping is used when ... 'entry' is executing at low address ?? p.22

	   (Btw, 4MB == 0x40_0000)
	*/
	[ 0 ]                    = ( 0 ) | PTE_P | PTE_W | PTE_PS,


	/* Map VA's [KERNBASE, KERNBASE + 4MB] to PA's [0, 4MB]

	   Maps high virtual address where kernel expects? to find its
	   instructions and data, to the low physical address where the
	   boot loader placed them.

	   This mapping is used when ... exit 'entry' and start executing at
	   high addresses...??

	   This mapping restricts the kernel instructions and rodata to 4 MB ??

	   (Btw, 0x8000_0000 / 4MB == 512 == KERNBASE >> 22)
	*/
	[ KERNBASE >> PDXSHIFT ] = ( 0 ) | PTE_P | PTE_W | PTE_PS,
};
