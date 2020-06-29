// Memory layout

/*
   KERNBASE limits physical memory used by xv6's to 2GB ??
*/

//
#define EXTMEM   0x100000    // Start of extended memory
#define PHYSTOP  0xE000000   // Top of physical memory (Note: stackoverflow.com/a/29892921)
#define DEVSPACE 0xFE000000  // Addresses used by memory mapped IO

// Key addresses for address space layout (see kmap in vm.c for layout)
#define KERNBASE 0x80000000             // First kernel virtual address
#define KERNLINK ( KERNBASE + EXTMEM )  // Address where kernel is linked

//
#define V2P( a ) ( ( ( uint ) ( a ) ) - KERNBASE )
#define P2V( a ) ( ( void* )( ( ( char* ) ( a ) ) + KERNBASE ) )

#define V2P_WO( x ) ( ( x ) - KERNBASE )  // same as V2P, but without casts
#define P2V_WO( x ) ( ( x ) + KERNBASE )  // same as P2V, but without casts


/* JK...
   Allow user space to read/write directly to specific IO devices
*/
// Make sure to pick values that keep USER_MMIO_BASE page-aligned
#define USER_MMIO_SIZE 0x00010000  // 16 * PGSIZE
#define USER_MMIO_BASE 0x7FFF0000  // KERNBASE - USER_MMIO_SIZE

// Virtual
#define UMMIO__VGA_MODE13_BUF ( USER_MMIO_BASE + 0 )

// Physical
#define VGA_MODE13_BUF_ADDR 0xA0000  // physical address
#define VGA_MODE13_BUF_SIZE 64000    // 320(w) * 200(h)


/*
TODO:
	. does the kernel have:
	    . a stack, yes many, per process stack, 'p->kstack' via 'allocproc'
	    . a heap?

	__ Virtual __

	                    0xFFFF_FFFF ->  -----------------------------
	                                    memory mapped
	                                    IO devices 
	         (DEVSPACE) 0xFE00_0000 ->  -----------------------------
	                                    unmapped/unused?
	                   P2V(PHYSTOP) ->  -----------------------------
	                                    ...
	                                    free memory
	                                    (used by?)
	                                    ...
	                     kernel.end ->  -----------------------------  <-
	                                    kernel uninitialized rwdata      |
	                     kernel.bss ->  -----------------------------    |
	                                    kernel initialized rwdata        |
	                    kernel.data ->  -----------------------------    |
	                                    kernel rodata                    |
	                  kernel.rodata ->  -----------------------------    |
	                                    kernel text                      |
	(KERNBASE + EXTMEM) 0x8010_0000 ->  -----------------------------    | kernel
	                                    ?                                |
	                    0x8000_7E00 ->  -----------------------------    |
	                                    boot sector                      |
	                    0x8000_7C00 ->  -----------------------------    |
	                                    ?                                |
	         (KERNBASE) 0x8000_0000 ->  -----------------------------  <-
	                                    vga mode13 framebuffer           |
	                 USER_MMIO_BASE ->  -----------------------------    |
	                                    ...                              |
	                                    free memory                      |
	                                    (used to grow user heap)         |
	                                    ...                              |
	                                    -----------------------------    |
	                                    user heap                        |
	                                    -----------------------------    |
	                 (n * PAGESIZE)     user stack                       | user
	                                    -----------------------------    |
	                     (PAGESIZE)     guard page                       |
	                                    -----------------------------    |
	                                    program data                     |
	                                    -----------------------------    |
	                                    program text                     |
	                    0x0000_0000 ->  -----------------------------  <-


	__ Physical __

	             0xFFFF_FFFF ->  -------------------------
	                             memory mapped
	                             IO devices
	  (DEVSPACE) 0xFE00_0000 ->  -------------------------
	                             unmapped/unused?
	                 PHYSTOP ->  -------------------------
	                             ...
	                             free memory
	                             (used by kalloc to allocate pages)
	                             ...
	         V2P(kernel.end) ->  -------------------------
	                             kernel initalized rwdata
	        V2P(kernel.data) ->  -------------------------
	                             kernel rodata
	                             -------------------------
	                             kernel text
	    (EXTMEM) 0x0010_0000 ->  -------------------------
	                             IO devices
	             0x000A_0000 ->  -------------------------
	                             ?
	             0x0000_7E00 ->  -------------------------
	                             boot sector (512 bytes)
	             0x0000_7C00 ->  -------------------------
	                             ?
	             0x0000_0000 ->  -------------------------


	. Boot loader (bootmain.S/c) loads xv6 kernel code at 0x0010_0000
	  to accomodate machines that might have small physical memory
	  (ex. less than 2GB)
*/
