// Memory layout

/*
   KERNBASE limits physical memory used by xv6's to 2GB ??
*/

#define EXTMEM   0x100000    // Start of extended memory
#define PHYSTOP  0xE000000   // Top of physical memory
#define DEVSPACE 0xFE000000  // Addresses used by memory mapped IO

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000             // First kernel virtual address
#define KERNLINK ( KERNBASE + EXTMEM )  // Address where kernel is linked

#define V2P( a ) ( ( ( uint ) ( a ) ) - KERNBASE )
#define P2V( a ) ( ( void* )( ( ( char* ) ( a ) ) + KERNBASE ) )

#define V2P_WO( x ) ( ( x ) - KERNBASE )  // same as V2P, but without casts
#define P2V_WO( x ) ( ( x ) + KERNBASE )  // same as P2V, but without casts

/*
TODO:
	. does the kernel have a stack, a heap?

	__ Virtual __

	                    0xFFFF_FFFF ->  -------------------------
	                                    memory mapped
	                                    IO devices 
	         (DEVSPACE) 0xFE00_0000 ->  -------------------------
	                                    ...
	                                    free memory
	                                    (unused?)
	                                    ...
	                    kernel.end ->   -------------------------  <-
	                                    kernel static data           |
	                                    -------------------------    |
	                                    kernel text                  |
	(KERNBASE + EXTMEM) 0x8010_0000 ->  -------------------------    | kernel
	                                    ?                            |
	                    0x8000_7E00 ->  -------------------------    |
	                                    boot sector                  |
	                    0x8000_7C00 ->  -------------------------    |
	                                    ?                            |
	         (KERNBASE) 0x8000_0000 ->  -------------------------  <-
	                                    ...                          |
	                                    free memory                  |
	                                    (used to grow user heap)     |
	                                    ...                          |
	                                    -------------------------    |
	                                    user heap                    |
	                                    -------------------------    |
	                     (PAGESIZE)     user stack                   | user
	                                    -------------------------    |
	                     (PAGESIZE)     guard page                   |
	                                    -------------------------    |
	                                    program static data          |
	                                    -------------------------    |
	                                    program text                 |
	                    0x0000_0000 ->  -------------------------  <-


	__ Physical __

	             0xFFFF_FFFF ->  -------------------
	                             memory mapped
	                             IO devices 
	               (PHYSTOP) ->  -------------------
	                             ...
	                             free memory
	                             (used to allocate pages)
	                             ...
	   kernel.end - KERNBASE ->  -------------------
	                             kernel static data
	                             -------------------
	                             kernel text
	    (EXTMEM) 0x0010_0000 ->  -------------------
	                             IO devices
	             0x000A_0000 ->  -------------------
	                             ?
	             0x0000_7E00 ->  -------------------
	                             boot sector (512 bytes)
	             0x0000_7C00 ->  -------------------
	                             ?
	             0x0000_0000 ->  -------------------


	. Boot loader (bootmain.S/c) loads xv6 kernel code at 0x0010_0000
	  to accomodate machines that might have small physical memory
	  (ex. less than 2GB)
*/
