// Memory layout

/*
   KERNBASE limits physical memory used by xv6's to 2GB ??
*/

#define EXTMEM   0x100000    // Start of extended memory
#define PHYSTOP  0xE000000   // Top physical memory
#define DEVSPACE 0xFE000000  // Other devices are at high addresses

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000             // First kernel virtual address
#define KERNLINK ( KERNBASE + EXTMEM )  // Address where kernel is linked

#define V2P( a ) ( ( ( uint ) ( a ) ) - KERNBASE )
#define P2V( a ) ( ( void* )( ( ( char* ) ( a ) ) + KERNBASE ) )

#define V2P_WO( x ) ( ( x ) - KERNBASE )  // same as V2P, but without casts
#define P2V_WO( x ) ( ( x ) + KERNBASE )  // same as P2V, but without casts

/*
	__ Virtual __

	                    0xFFFF_FFFF ->  -------------------
	                                    memory mapped
	                                    IO devices 
	         (DEVSPACE) 0xFE00_0000 ->  -------------------
	                                    ...
	                                    free memory (used to allocate pages)
	                                    ...
	                    0x8040_0000 ->  -------------------  <-
	                                    kernel rodata          |
	                                    -------------------    |
	                                    kernel text            |
	(KERNBASE + EXTMEM) 0x8010_0000 ->  -------------------    | kernel
	                                    bootloader, ?          |
	         (KERNBASE) 0x8000_0000 ->  -------------------  <-
	                                    user heap              |
	                                    -------------------    |
	                     (PAGESIZE)     user stack             | user
	                                    -------------------    |
	                                    user rodata            |
	                                    -------------------    |
	                                    user text              |
	                    0x0000_0000 ->  -------------------  <-


	__ Physical __

	             0xFFFF_FFFF ->  -------------------
	                             memory mapped
	                             IO devices 
	             (PHYSTOP) ? ->  -------------------
	                             ...
	                             free memory
	                             ...
	             0x0040_0000 ->  -------------------
	                             kernel rodata
	                             -------------------
	                             kernel text
	    (EXTMEM) 0x0010_0000 ->  -------------------
	                             IO devices
	             0x000A_0000 ->  -------------------
	                             ?
	             0x0000_7E00 ->  -------------------
	                             boot loader (512 bytes)
	             0x0000_7C00 ->  -------------------
	                             ?
	             0x0000_0000 ->  -------------------


	. Boot loader loads xv6 kernel code at 0x0010_0000 to accomodate machines
	  that might have small physical memory (ex. less than 2GB)
*/
