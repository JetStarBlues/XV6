#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data [];  // defined by kernel.ld

pde_t* kpgdir;  // for use in scheduler()

struct _mmap
{
	void* virt_start;
	uint  phys_start;
	uint  phys_end;     // exclusive
	int   permissions;

};


// _________________________________________________________________________________

// Set up CPU's kernel segment descriptors.
// Run once on entry on each CPU.
void seginit ( void )
{
	struct cpu* c;

	// Map "logical" addresses to virtual addresses using identity map.
	// Cannot share a CODE descriptor for both kernel and user
	// because it would have to have DPL_USR, but the CPU forbids
	// an interrupt from CPL=0 to DPL=3.
	c = &cpus[ cpuid() ];

	c->gdt[ SEG_KCODE ] = SEG( STA_X | STA_R, 0, 0xffffffff, DPL_KERN );
	c->gdt[ SEG_KDATA ] = SEG( STA_W,         0, 0xffffffff, DPL_KERN );
	c->gdt[ SEG_UCODE ] = SEG( STA_X | STA_R, 0, 0xffffffff, DPL_USER );
	c->gdt[ SEG_UDATA ] = SEG( STA_W,         0, 0xffffffff, DPL_USER );

	lgdt( c->gdt, sizeof( c->gdt ) );
}


// _________________________________________________________________________________

/* Return the address of the PTE in page table pgdir
   that corresponds to virtual address vAddr.
   If alloc != 0, create any required page table pages.

   Mimics actions of x86 paging hardware
*/
static pte_t* walkpgdir ( pde_t* pgdir, const void* vAddr, int alloc )
{
	pde_t* pde;
	pte_t* pgtab;

	// Get page directory entry
	//  PDE index is upper 10 bits of virtual address,
	//  PDE address is pgdir[ PDE index ]
	pde = &( pgdir[ PDX( vAddr ) ] );

	// If the PDE is present, retrieve relevant page table
	if ( *pde & PTE_P )
	{
		pgtab = ( pte_t* ) P2V( PTE_ADDR( *pde ) );
	}

	// If the PDE is not present, the required page table
	// has not yet been allocated
	else
	{
		// If the alloc argument is set, allocate a page table
		if ( ! alloc )
		{
			return 0;
		}

		pgtab = ( pte_t* ) kalloc();

		if ( pgtab == 0 )
		{
			return 0;
		}

		// Make sure all those PTE_P bits are zero.
		memset( pgtab, 0, PGSIZE );

		// The permissions here are overly generous, but they can
		// be further restricted by the permissions in the page table
		// entries, if necessary.
		*pde = V2P( pgtab ) | PTE_P | PTE_W | PTE_U;
	}


	// Get page table entry
	//  PTE index is second upper 10 bits of virtual address,
	//  PTE address is pgtab[ PTE index ]
	return &( pgtab[ PTX( vAddr ) ] );
}

/* Create PTEs for virtual addresses starting at vAddr that refer to
   physical addresses starting at pAddr.

   vAddr and size might not be page-aligned.
*/
static int mappages ( pde_t* pgdir, void* vAddr, uint pAddr, uint size, int permissions )
{
	char*  a;
	char*  last;
	pte_t* pte;

	// Page align vAddr start and vAddr end
	a    = ( char* ) PGROUNDDOWN(   ( uint ) vAddr );
	last = ( char* ) PGROUNDDOWN( ( ( uint ) vAddr ) + size - 1 );

	// cprintf( "mappages first %8x, %8x\n", a, pAddr );

	for ( ;; )
	{
		// Retrieve the virtual address's page table entry
		pte = walkpgdir( pgdir, a, 1 );

		if ( pte == 0 )
		{
			return - 1;
		}

		// Check if already mapped to a physical page
		/*if ( *pte & PTE_P )
		{
			panic( "mappages: remap" );
		}*/

		// JK- check if already mapped to a non-IO physical page
		if ( ( *pte & PTE_P ) &&
		     (
		       ( a <  ( char* ) USER_MMIO_BASE ) ||
		       ( a >= ( char* ) KERNBASE )
		     )
		)
		{
			panic( "mappages: remap" );
		}

		// Set PTE's physical page number, permissions, and present/valid bit
		*pte = pAddr | permissions | PTE_P;


		//
		if ( a == last )
		{
			// cprintf( "mappages last  %8x, %8x\n", a, pAddr );
			break;
		}

		a     += PGSIZE;
		pAddr += PGSIZE;
	}

	return 0;
}


// _________________________________________________________________________________

/* There is one page table per process, plus one that's used when
   a CPU is not running any process (kpgdir). The kernel uses the
   current process's page table during system calls and interrupts;
   page protection bits prevent user code from using the kernel's
   mappings.

   setupkvm() and exec() set up every page table like this:

     0 .. KERNBASE:
         . user memory (text, data, stack, heap)
         . mapped to physical memory allocated to user by the kernel

     KERNBASE .. KERNBASE + EXTMEM:
         . "I/O space"

     KERNBASE + EXTMEM .. data:
         . kernel's instructions and kernel's r/o data

     data .. P2V(PHYSTOP):
         . kernel's rw data and free physical memory

     DEVSPACE .. 0xFFFF_FFFF:
         . memory-mapped devices such as ioapic
         . mapped directly (virtual address == physical address)

   The kernel allocates physical memory for its heap and for user memory
   between V2P( end ) and the end of physical memory (PHYSTOP)
   (directly addressable from end..P2V( PHYSTOP )).
*/

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct _mmap kmap [] = {

	// "I/O space"
	/*
		Virtual range :
			start : KERNBASE
			end   : KERNBASE + EXTMEM
		Physical range :
			start : 0
			end   : 0 + EXTMEM
	*/
	{
		( void* ) KERNBASE,
		0,
		EXTMEM,
		PTE_W
	},

	// Kernel text and kernel rodata
	/*
		Virtual range :
			start : KERNBASE + EXTMEM
			end   : KERNBASE + EXTMEM + sizeof( kernel text and rodata )
		Physical range :
			start : 0 + EXTMEM
			end   : 0 + EXTMEM + sizeof( kernel text and rodata )
	*/
	{
		( void* ) KERNLINK,
		V2P( KERNLINK ),
		V2P( data ),
		0                    // read only
	},

	// Kernel rwdata and free memory
	/*
		Virtual range :
			start : KERNBASE + EXTMEM + sizeof( kernel text and rodata )
			end   : P2V(PHYSTOP)
		Physical range :
			start : 0 + EXTMEM + sizeof( kernel text and rodata )
			end   : PHYSTOP
	*/
	{
		( void* ) data,
		V2P( data ),
		PHYSTOP,
		PTE_W
	},

	// Memory mapped devices
	/*
		Virtual range :
			start : DEVSPACE
			end   : 0xFFFF_FFFF
		Physical range :
			start : DEVSPACE
			end   : 0xFFFF_FFFF
	*/
	{
		( void* ) DEVSPACE,
		DEVSPACE,  /* DEVSPACE and not V2P( DEVSPACE ) because
		              doesn't actually go to physical RAM.
		              Instead goes to a device...
		           */
		0,         /* 0xFFFF_FFFF + 1 because treating phys_end
		              as start of next, for size calculations phys_end - phys_start
		           */
		PTE_W
	}
};

/* JK...
   Allow user-space to read/write directly to specific IO devices.
   This mapping is present in every process's page table.
*/
static struct _mmap umap [] = {

	// VGA Mode 0x13 frame buffer
	/*
		Virtual range :
			start : USER_MMIO_BASE
			end   : USER_MMIO_BASE + VGA_MODE13_BUF_SIZE
		Physical range :
			start : VGA_MODE13_BUF_ADDR
			end   : VGA_MODE13_BUF_ADDR + VGA_MODE13_BUF_SIZE
	*/
	{
		( void* ) USER_MMIO_BASE,
		VGA_MODE13_BUF_ADDR,
		VGA_MODE13_BUF_ADDR + VGA_MODE13_BUF_SIZE,
		PTE_W | PTE_U
	}
};

// Create a page table? and map the kernel part.
/* Causes all processes' page tables to have identical mappings
   for kernel code and rodata... 
   ??
*/
pde_t* setupkvm ( void )
{
	struct _mmap* mp;
	pde_t*        pgdir;

	// Allocate a page of memory to hold the page directory
	pgdir = ( pde_t* ) kalloc();

	if ( pgdir == 0 )
	{
		return 0;
	}

	// Clear junk
	memset( pgdir, 0, PGSIZE );


	// Check if using region reserved for memory mapped IO
	if ( P2V( PHYSTOP ) > ( void* ) DEVSPACE )
	{
		panic( "setupkvm: PHYSTOP too high" );
	}


	// Map kernel virtual addresses (KERNBASE..0xFFFF_FFFF)
	for ( mp = kmap; mp < &( kmap[ NELEM( kmap ) ] ); mp += 1 )
	{
		/*cprintf(

			"kmp: %8x -- %8x, %8x, %8x\n",
			mp->virt_start,
			mp->phys_start, mp->phys_end,
			mp->phys_end - mp->phys_start
		);*/

		if (
			mappages(

				pgdir,
				mp->virt_start,                 // virtual start address
				( uint ) mp->phys_start,        // physical start address
				mp->phys_end - mp->phys_start,  // size
				mp->permissions
			) < 0 )
		{
			freevm( pgdir );

			return 0;
		}
	}


	/* JK...
	   Map user IO virtual addresses (USER_MMIO_BASE..KERNBASE).
	   Over here for convenience... can be separate function.
	*/
	for ( mp = umap; mp < &( umap[ NELEM( umap ) ] ); mp += 1 )
	{
		/*cprintf(

			"ump: %8x -- %8x, %8x, %8x\n",
			mp->virt_start,
			mp->phys_start, mp->phys_end,
			mp->phys_end - mp->phys_start
		);*/

		if (
			mappages(

				pgdir,
				mp->virt_start,                 // virtual start address
				( uint ) mp->phys_start,        // physical start address
				mp->phys_end - mp->phys_start,  // size
				mp->permissions
			) < 0 )
		{
			freevm( pgdir );

			return 0;
		}
	}


	return pgdir;
}


// _________________________________________________________________________________

// Switch h/w page table register to the kernel-only page table,
// for when no process is running.
void switchkvm ( void )
{
	lcr3( V2P( kpgdir ) );  // switch to the kernel page table
}

// Switch h/w page table and TSS to correspond to process p.
/* Tell the CPU to start using the target process's page table.
 
   Also setup up a task state segment (SEG_TSS) that instructs
   the CPU to execute system calls and interrupts on the process's
   kernel stack...
*/
/* When changing protection levels from user to kernel mode (i.e.
   when preparing to execute a system call), the kernel shouldn't
   use the user stack. The user stack may be malicious or contain
   an error that causes the user %esp to hold an address that is
   not part of the process's user memory...

   xv6 programs the x86 CPU to, on a trap (since privilege levels
   might change), perform a stack switch (from the user stack to the
   kernel stack) 
   It does this by setting up a task segment descriptor through which
   the CPU loads a stack segment selector (%ss) and stack pointer (%esp)

   switchuvm stores the address of the top of the kernel stack of the
   user process into the task segment descriptor...
*/
void switchuvm ( struct proc* p )
{
	if ( p == 0 )
	{
		panic( "switchuvm: no process" );
	}

	if ( p->kstack == 0 )
	{
		panic( "switchuvm: no kstack" );
	}

	if ( p->pgdir == 0 )
	{
		panic( "switchuvm: no pgdir" );
	}

	// ?
	pushcli();


	// ?
	mycpu()->gdt[ SEG_TSS ] = SEG16(

		STS_T32A,                   // type
		&mycpu()->ts,               // base address
		sizeof( mycpu()->ts ) - 1,  // limit
		0                           // descriptor privilege level
	);

	mycpu()->gdt[ SEG_TSS ].s = 0;  // 0 = system, 1 = application


	// Point to kernel stack
	mycpu()->ts.ss0  = SEG_KDATA << 3;
	mycpu()->ts.esp0 = ( uint ) p->kstack + KSTACKSIZE;  // top of kernel stack


	// Setting IOPL=0 in eflags *and* iomb beyond the tss segment limit
	// forbids I/O instructions (e.g. inb and outb) from user space
	mycpu()->ts.iomb = ( ushort ) 0xFFFF;


	// load...? update TSS ??
	ltr( SEG_TSS << 3 );


	// Switch to the process's page table...
	lcr3( V2P( p->pgdir ) );


	// ?
	popcli();
}


// _________________________________________________________________________________

// Allocate one page table for the machine for the kernel address
// space for scheduler processes.
// ??
void kvmalloc ( void )
{
	kpgdir = setupkvm();

	switchkvm();
}


// _________________________________________________________________________________

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.

/* Called by userinit to...
*/
void inituvm ( pde_t* pgdir, char* init, uint sz )
{
	char* mem;

	if ( sz >= PGSIZE )
	{
		panic( "inituvm: more than a page" );
	}

	// Allocate one page of physical memory
	mem = kalloc();

	// Clear junk from the page
	memset( mem, 0, PGSIZE );

	// Map virtual address 0 to the page's physical address
	mappages(

		pgdir,
		0,             // virtual start address
		V2P( mem ),    // physical start address
		PGSIZE,        // size
		PTE_W | PTE_U
	);

	// Copy the initcode binary to the page
	memmove( mem, init, sz );
}


// _________________________________________________________________________________

// Load a program segment into pgdir. vAddr must be page-aligned
// and the pages from vAddr to vAddr+size must already be mapped.

/* Called by exec to...
*/
int loaduvm ( pde_t* pgdir, char* vAddr, struct inode* ip, uint offset, uint size )
{
	uint   i,
	       pAddr,
	       n;
	pte_t* pte;

	if ( ( uint ) vAddr % PGSIZE != 0 )
	{
		panic( "loaduvm: vAddr must be page aligned" );
	}

	for ( i = 0; i < size; i += PGSIZE )
	{
		/* Get physical address of the allocated memory
		   at which to write...
		*/
		pte = walkpgdir( pgdir, vAddr + i, 0 );

		if ( pte == 0 )
		{
			panic( "loaduvm: address should exist" );
		}

		pAddr = PTE_ADDR( *pte );  // physical address


		//
		if ( size - i < PGSIZE )
		{
			n = size - i;
		}
		else
		{
			n = PGSIZE;
		}


		// Read from file into memory...
		if ( readi( ip, P2V( pAddr ), offset + i, n ) != n )
		{
			return - 1;
		}
	}

	return 0;
}


// _________________________________________________________________________________

// Given a parent process's page table, create a copy
// of it for a child.

/* Called by fork to...
*/
pde_t* copyuvm ( pde_t* pgdir, uint sz )
{
	pde_t* newPgdir;  // copy of pgdir
	pte_t* pte;
	uint   pAddr,
	       i,
	       flags;
	char*  mem;

	newPgdir = setupkvm();

	if ( newPgdir == 0 )
	{
		return 0;
	}

	for ( i = 0; i < sz; i += PGSIZE )
	{
		pte = walkpgdir( pgdir, ( void* ) i, 0 );

		if ( pte == 0 )
		{
			panic( "copyuvm: pte should exist" );
		}

		if ( ! ( *pte & PTE_P ) )
		{
			panic( "copyuvm: page not present" );
		}

		pAddr = PTE_ADDR(  *pte );
		flags = PTE_FLAGS( *pte );

		mem = kalloc();

		if ( mem == 0 )
		{
			goto bad;
		}

		memmove( mem, ( char* ) P2V( pAddr ), PGSIZE );

		if (
			mappages(

				newPgdir,
				( void* ) i,  // virtual start address
				V2P( mem ),   // physical start address
				PGSIZE,       // size
				flags
			) < 0 )
		{
			kfree( mem );

			goto bad;
		}
	}


	// JK - add user-space IO mappings
	/*if ( map_uio( pgdir ) < 0 )
	{
		goto bad;
	}*/


	//
	return newPgdir;

bad:

	freevm( newPgdir );

	return 0;
}


// _________________________________________________________________________________

/* Allocate page tables and physical memory to grow process from oldsz to
   newsz, which need not be page aligned.
   Returns new size or 0 on error.
*/
/*
   Allocate physical pages and map them to the process's
   address space.
*/
int allocuvm ( pde_t* pgdir, uint oldsz, uint newsz )
{
	char* mem;
	uint  a;

	// Upper limit
	// if ( newsz >= KERNBASE )
	if ( newsz >= USER_MMIO_BASE )
	{
		return 0;
	}

	// To shrink, should call deallocuvm
	if ( newsz < oldsz )
	{
		return oldsz;
	}

	a = PGROUNDUP( oldsz );

	for ( ; a < newsz; a += PGSIZE )
	{
		mem = kalloc();

		if ( mem == 0 )
		{
			cprintf( "allocuvm: out of memory\n" );

			deallocuvm( pgdir, newsz, oldsz );

			return 0;
		}

		// Zero the memory
		/* Why?
		    . clean garbage
		    . C assumes that unitialized statics (BSS section)
		      have a value of zero...
		    . security (can't read another process's old data)
		*/
		memset( mem, 0, PGSIZE );

		// ...
		if (
			mappages(

				pgdir,
				( char* ) a,   // virtual start address
				V2P( mem ),    // physical start address
				PGSIZE,        // size
				PTE_W | PTE_U
			) < 0 )
		{
			cprintf( "allocuvm: out of memory (2)\n" );

			deallocuvm( pgdir, newsz, oldsz );

			kfree( mem );

			return 0;
		}
	}

	return newsz;
}

/* Deallocate user pages to bring the process size from
   oldsz to newsz.
   . oldsz and newsz need not be page-aligned, nor does newsz
     need to be less than oldsz.
   . oldsz can be larger than the actual process size.
   Returns the new process size.
*/
/*
   Unmap pages from the process's address space and free
   underlying physical memory.
*/
int deallocuvm ( pde_t* pgdir, uint oldsz, uint newsz )
{
	pte_t* pte;
	uint   a,
	       pAddr;
	char*  vAddr;

	// To grow, should call allocuvm
	if ( newsz >= oldsz )
	{
		return oldsz;
	}

	a = PGROUNDUP( newsz );

	for ( ; a < oldsz; a += PGSIZE )
	{
		pte = walkpgdir( pgdir, ( char* ) a, 0 );

		// ?
		if ( ! pte )
		{
			a = PGADDR( PDX( a ) + 1, 0, 0 ) - PGSIZE;
		}
		// Free corresponding physical page...
		else if ( ( *pte & PTE_P ) != 0 )
		{
			pAddr = PTE_ADDR( *pte );

			if ( pAddr == 0 )
			{
				panic( "deallocuvm: kfree" );
			}

			vAddr = P2V( pAddr );

			kfree( vAddr );

			*pte = 0;
		}
	}

	return newsz;
}


// _________________________________________________________________________________

// Free a page table and all the physical memory pages
// in the user part.
void freevm ( pde_t* pgdir )
{
	uint  i;
	char* vAddr;

	if ( pgdir == 0 )
	{
		panic( "freevm: no pgdir" );
	}


	// Free memory pointed to by page table
	// deallocuvm( pgdir, KERNBASE, 0 );  // region 0..KERNBASE
	deallocuvm( pgdir, USER_MMIO_BASE, 0 );  // region 0..USER_MMIO_BASE


	// Free memory used by page table
	for ( i = 0; i < NPDENTRIES; i += 1 )
	{
		if ( pgdir[ i ] & PTE_P )
		{
			vAddr = P2V( PTE_ADDR( pgdir[ i ] ) );

			kfree( vAddr );
		}
	}

	kfree( ( char* ) pgdir );
}


// _________________________________________________________________________________

// Clear PTE_U on a page. Used to create an inaccessible
// page beneath the user stack.
void clearpteu ( pde_t* pgdir, char* uva )
{
	pte_t* pte;

	pte = walkpgdir( pgdir, uva, 0 );

	if ( pte == 0 )
	{
		panic( "clearpteu" );
	}

	*pte &= ~ PTE_U;
}


// _________________________________________________________________________________

// Map user virtual address to kernel address.
/* Check that virtual address is mapped to user space.
   If it is, return address of the associated physical page.
*/
char* uva2ka ( pde_t* pgdir, char* uva )
{
	pte_t* pte;

	pte = walkpgdir( pgdir, uva, 0 );

	if ( ( *pte & PTE_P ) == 0 )
	{
		return 0;
	}

	if ( ( *pte & PTE_U ) == 0 )
	{
		return 0;
	}

	return ( char* ) P2V( PTE_ADDR( *pte ) );
}

// Copy len bytes from p to user address vAddr in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
// Used by exec to copy arguments to the user stack...
int copyout ( pde_t* pgdir, uint vAddr, void* p, uint len )
{
	char* buf;
	char* pAddr0;
	uint  n,
	      vAddr0;

	buf = ( char* ) p;

	while ( len > 0 )
	{
		vAddr0 = ( uint ) PGROUNDDOWN( vAddr );

		pAddr0 = uva2ka( pgdir, ( char* ) vAddr0 );

		if ( pAddr0 == 0 )
		{
			return - 1;
		}

		n = PGSIZE - ( vAddr - vAddr0 );

		if ( n > len )
		{
			n = len;
		}

		memmove( pAddr0 + ( vAddr - vAddr0 ), buf, n );

		len -= n;
		buf += n;

		vAddr = vAddr0 + PGSIZE;
	}

	return 0;
}
