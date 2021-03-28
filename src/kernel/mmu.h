// This file contains definitions for the
// x86 memory management unit (MMU).

// Eflags register
#define FL_IF 0x00000200    // Interrupt Enable

// Control Register flags
#define CR0_PE  0x00000001  // Protection Enable
#define CR0_WP  0x00010000  // Write Protect
#define CR0_PG  0x80000000  // Paging

#define CR4_PSE 0x00000010  // Page size extension


// ____________________________________________________________________________

// various segment selectors.
#define SEG_KCODE 1  // kernel code
#define SEG_KDATA 2  // kernel data+stack
#define SEG_UCODE 3  // user code
#define SEG_UDATA 4  // user data+stack
#define SEG_TSS   5  // this process's task state

// cpu->gdt[NSEGS] holds the above segments.
#define NSEGS     6

#ifndef __ASSEMBLER__

/*
	Diagram from Intel 80386 DX datasheet

	TODO p.37
*/

// Segment Descriptor
struct segdesc
{
	uint lim_15_0   : 16;  // Low bits of segment limit
	uint base_15_0  : 16;  // Low bits of segment base address
	uint base_23_16 : 8;   // Middle bits of segment base address
	uint type       : 4;   // Segment type (see STS_ constants)
	uint s          : 1;   // 0 = system, 1 = application
	uint dpl        : 2;   // Descriptor Privilege Level
	uint p          : 1;   // Present
	uint lim_19_16  : 4;   // High bits of segment limit
	uint avl        : 1;   // Unused (available for software use)
	uint rsv1       : 1;   // Reserved
	uint db         : 1;   // 0 = 16-bit segment, 1 = 32-bit segment
	uint g          : 1;   // Granularity: limit scaled by 4K when set
	uint base_31_24 : 8;   // High bits of segment base address
};

// Normal segment
#define SEG( type, base, lim, dpl ) ( struct segdesc )   \
{                                                        \
	( ( lim ) >> 12 ) & 0xffff,                          \
	( uint )( base ) & 0xffff,                           \
	( ( uint )( base ) >> 16 ) & 0xff,                   \
	type,                                                \
	1,                                                   \
	dpl,                                                 \
	1,                                                   \
	( uint )( lim ) >> 28,                               \
	0,                                                   \
	0,                                                   \
	1,                                                   \
	1,                                                   \
	( uint )( base ) >> 24                               \
}

#define SEG16( type, base, lim, dpl ) ( struct segdesc )   \
{                                                          \
	( lim ) & 0xffff,                                      \
	( uint )( base ) & 0xffff,                             \
	( ( uint )( base ) >> 16 ) & 0xff,                     \
	type,                                                  \
	1,                                                     \
	dpl,                                                   \
	1,                                                     \
	( uint )( lim ) >> 16,                                 \
	0,                                                     \
	0,                                                     \
	1,                                                     \
	0,                                                     \
	( uint )( base ) >> 24                                 \
}

#endif

#define DPL_KERN 0x0  // Kernel DPL (JK)
#define DPL_USER 0x3  // User DPL

// Application segment type bits
#define STA_X    0x8  // Executable segment
#define STA_W    0x2  // Writeable (non-executable segments)
#define STA_R    0x2  // Readable (executable segments)

// System segment type bits
#define STS_T32A 0x9  // Available 32-bit TSS
#define STS_IG32 0xE  // 32-bit Interrupt Gate
#define STS_TG32 0xF  // 32-bit Trap Gate


// ____________________________________________________________________________

/*
	A virtual address 'va' has a three-part structure as follows:

	+--------10---------+-------10----------+---------12----------+
	| Page Directory    |   Page Table      | Offset within Page  |
	|      Index        |      Index        |                     |
	+-------------------+-------------------+---------------------+
	 \--- PD_IDX(va) --/ \--- PT_IDX(va) --/
*/

// page directory index
#define PD_IDX( va ) ( ( ( uint )( va ) >> PD_IDX_SHIFT ) & 0x3FF )

// page table index
#define PT_IDX( va ) ( ( ( uint )( va ) >> PT_IDX_SHIFT ) & 0x3FF )

// construct virtual address from indexes and offset
#define PGADDR( pd_idx, pt_idx, offset ) ( ( uint ) ( ( ( pd_idx ) << PD_IDX_SHIFT ) | ( ( pt_idx ) << PT_IDX_SHIFT ) | ( offset ) ) )

// Page directory and page table constants.
#define NPDENTRIES 1024  // # directory entries per page directory
#define NPTENTRIES 1024  // # PTEs per page table
#define PGSIZE     4096  // bytes mapped by a page

#define PT_IDX_SHIFT 12  // offset of PT_IDX in a linear address
#define PD_IDX_SHIFT 22  // offset of PD_IDX in a linear address

#define PGROUNDUP( a )   ( ( ( a ) + PGSIZE - 1 ) & ( ~ ( PGSIZE - 1 ) ) )
#define PGROUNDDOWN( a ) (              ( ( a ) ) & ( ~ ( PGSIZE - 1 ) ) )


/*
	See https://wiki.osdev.org/Paging

	xv6-book figure 2-1:
		"
		Page directory and page table entries are identical except for
		the Dirty bit (which is always zero in a PDE)
		"

	Page directory entry:

		31..12 - physical address of page table
		11..9  - unused
		    8  - flag | unused          | .
		    7  - flag | page size       | If 0, page has size of 4KiB. If 1, 4MiB.
		    6  - flag | unused          | .
		    5  - flag | accessed        | If 1, page has been accessed for read or write. Up to OS to clear the bit.
		    4  - flag | cache disabled  | If 0, page cache is enabled
		    3  - flag | write through   | If 1, write-through caching is enabled. If 0, write-back caching
		    2  - flag | supervisor/user | If 0, only supervisor can access page
		    1  - flag | read/write      | If 0, page is read-only
		    0  - flag | present         | If 1, page is actually in physical memory. For ex, when a page is
		                                | swapped out, it is no longer in physical memory. When a page is called,
		                                | but it is not present, a page fault will occur, and OS should handle it.

	Page table entry:

		31..12 - physical address of page_size block of memory
		11..9  - unused
		    8  - flag | don't care      | .
		    7  - flag | don't care      | .
		    6  - flag | dirty           | If 1, indicates page has been written to. Up to OS to clear the bit.
		    5  - flag | accessed        | If 1, page has been accessed for read or write. Up to OS to clear the bit.
		    4  - flag | cache disabled  | If 0, page cache is enabled
		    3  - flag | write through   | If 1, write-through caching is enabled. If 0, write-back caching
		    2  - flag | supervisor/user | If 0, only supervisor can access page
		    1  - flag | read/write      | If 0, page is read-only
		    0  - flag | present         | If 1, page is actually in physical memory. For ex, when a page is
		                                | swapped out, it is no longer in physical memory. When a page is called,
		                                | but it is not present, a page fault will occur, and OS should handle it.
*/

// Page directory entry flags
#define PDE_P  0x01   // Present
#define PDE_W  0x02   // Writeable
#define PDE_U  0x04   // User
#define PDE_PS 0x80   // Page Size. Enables 4Mbyte "super" page

// Page table entry flags
#define PTE_P  0x01   // Present
#define PTE_W  0x02   // Writeable
#define PTE_U  0x04   // User

// Address in page directory entry
#define PDE_ADDR( pde )  ( ( uint ) ( pde ) & ~ 0xFFF )
#define PDE_FLAGS( pde ) ( ( uint ) ( pde ) &   0xFFF )

// Address in page table entry
#define PTE_ADDR( pte )  ( ( uint ) ( pte ) & ~ 0xFFF )
#define PTE_FLAGS( pte ) ( ( uint ) ( pte ) &   0xFFF )


#ifndef __ASSEMBLER__

typedef uint pte_t;  // Why is this here, and not in 'types.h'?

#endif


// ____________________________________________________________________________

#ifndef __ASSEMBLER__

// Task state segment format
/* TSS originally intended to support hardware task switching
   in x86 CPUs.

   We only use it for switching stacks when crossing from user
   to kernel privilege.

   I.e. only relevant fields are:
      taskstate->esp0
      taskstate->ss0

   The other values of interest are pushed to the kernel stack
   by 'int' instruction and 'alltraps'
*/
/* For details see,
    Chapter 16, Task Management,
    IA-32 Intel Architecture Software Developer’s Manual, Volume 1,
    https://www.cs.umd.edu/~hollings/cs412/s02/proj1/ia32ch7.pdf
*/
struct taskstate
{
	uint    link;       // Old ts selector  ??

	uint    esp0;       // Stack pointer and segment selector
	ushort  ss0;        //  after an increase in privilege level to kernel (ring 0)
	ushort  padding1;

	uint*   esp1;       // Ditto (ring1)
	ushort  ss1;
	ushort  padding2;

	uint*   esp2;       // Ditto (ring2)
	ushort  ss2;
	ushort  padding3;

	void*   cr3;        // Page directory base

	uint*   eip;        // Saved state from last task switch
	uint    eflags;

	uint    eax;        // More saved state (registers)
	uint    ecx;
	uint    edx;
	uint    ebx;
	uint*   esp;
	uint*   ebp;
	uint    esi;
	uint    edi;

	ushort  es;         // Even more saved state (segment selectors)
	ushort  padding4;
	ushort  cs;
	ushort  padding5;
	ushort  ss;
	ushort  padding6;
	ushort  ds;
	ushort  padding7;
	ushort  fs;
	ushort  padding8;
	ushort  gs;
	ushort  padding9;

	ushort  ldt;        // ldt segment selector
	ushort  padding10;
	ushort  t;          // debug flag. Raise a "task-switch" debug exception...
	ushort  iomb;       // I/O map base address
};

// Gate descriptors for interrupts and traps
// See https://en.wikibooks.org/wiki/X86_Assembly/Advanced_Interrupts
struct gatedesc
{
	uint off_15_0  : 16;  // low 16 bits of offset in segment
	uint cs        : 16;  // code segment selector
	uint args      : 5;   // # args, 0 for interrupt/trap gates
	uint rsv1      : 3;   // reserved (should be zero I guess)
	uint type      : 4;   // type(STS_{IG32,TG32})
	uint s         : 1;   // must be 0 (system)
	uint dpl       : 2;   // descriptor(meaning new) privilege level
	uint p         : 1;   // Present
	uint off_31_16 : 16;  // high bits of offset in segment
};

// Set up a normal interrupt/trap gate descriptor.
/* - istrap : 1 for a trap (exception) gate, 0 for an interrupt gate.
              interrupt gate clears FL_IF, trap gate leaves FL_IF alone
   - sel    : Code segment selector for interrupt/trap handler
   - off    : Offset in code segment for interrupt/trap handler
   - d      : Descriptor Privilege Level -
               the privilege level required for software to invoke
               this interrupt/trap gate explicitly using an int instruction.
*/
#define SETGATE( gate, istrap, sel, off, d )                \
{                                                           \
	( gate ).off_15_0  = ( uint )( off ) & 0xffff;          \
	( gate ).cs        = ( sel );                           \
	( gate ).args      = 0;                                 \
	( gate ).rsv1      = 0;                                 \
	( gate ).type      = ( istrap ) ? STS_TG32 : STS_IG32;  \
	( gate ).s         = 0;                                 \
	( gate ).dpl       = ( d );                             \
	( gate ).p         = 1;                                 \
	( gate ).off_31_16 = ( uint )( off ) >> 16;             \
}

#endif
