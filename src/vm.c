#include "param.h"
#include "types.h"
#include "defs.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "elf.h"

extern char data [];  // defined by kernel.ld
pde_t*      kpgdir;   // for use in scheduler()

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

// Return the address of the PTE in page table pgdir
// that corresponds to virtual address va. If alloc != 0,
// create any required page table pages.
// Mimics actions of x86 paging hardware
static pte_t* walkpgdir ( pde_t* pgdir, const void* va, int alloc )
{
	pde_t* pde;
	pte_t* pgtab;

	// Get page directory entry
	//  PDE index is upper 10 bits of virtual address,
	//  PDE address is pgdir[ PDE index ]
	pde = &pgdir[ PDX( va ) ];

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
		if ( ! alloc || ( pgtab = ( pte_t* ) kalloc() ) == 0 )
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
	return &pgtab[ PTX( va ) ];
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned.
static int mappages ( pde_t* pgdir, void* va, uint size, uint pa, int permissions )
{
	char*  a;
	char*  last;
	pte_t* pte;

	a    = ( char* ) PGROUNDDOWN(   ( uint ) va );
	last = ( char* ) PGROUNDDOWN( ( ( uint ) va ) + size - 1 );

	for ( ;; )
	{
		// Retrieve the virtual address's page table entry
		if ( ( pte = walkpgdir( pgdir, a, 1 ) ) == 0 )
		{
			return - 1;
		}

		if ( *pte & PTE_P )
		{
			panic( "remap" );
		}

		// Set PTE's physical page number, permissions, and present/valid bit
		*pte = pa | permissions | PTE_P;


		//
		if ( a == last )
		{
			break;
		}

		a  += PGSIZE;
		pa += PGSIZE;
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
         . mapped to 0 .. EXTMEM
 
     KERNBASE + EXTMEM .. data:
         . kernel's instructions and kernel's r/o data
         . mapped to EXTMEM .. V2P( data )
 
     data .. KERNBASE + PHYSTOP:
         . kernel's r/w data and free physical memory
         . mapped to V2P( data ) .. PHYSTOP

     DEVSPACE .. 0:
         . memory-mapped devices such as ioapic
         . mapped directly (virtual address == physical address)
  
   The kernel allocates physical memory for its heap and for user memory
   between V2P( end ) and the end of physical memory (PHYSTOP)
   (directly addressable from end..P2V( PHYSTOP )).
*/

// This table defines the kernel's mappings, which are present in
// every process's page table.
static struct kmap
{
	void* virt;
	uint  phys_start;
	uint  phys_end;
	int   perm;

} kmap [] = {

	// "I/O space"
	{
		( void* ) KERNBASE,
		0,
		EXTMEM,
		PTE_W
	},
	// Kernel text and kernel rodata
	{
		( void* ) KERNLINK,
		V2P( KERNLINK ),
		V2P( data ),
		0
	},
	// Kernel rwdata and free memory
	{
		( void* ) data,
		V2P( data ),
		PHYSTOP,
		PTE_W
	},
	// Memory mapped devices
	{
		( void* ) DEVSPACE,
		DEVSPACE,
		0,          // why phys_end of zero ??
		PTE_W
	}
};

// Set up kernel part of a page table.
/* Causes all processes' page tables to have identical mappings
   for kernel code and rodata... 
   ??
*/
pde_t* setupkvm ( void )
{
	struct kmap* k;
	pde_t*       pgdir;

	// Allocate a page of memory to hold the page directory
	if ( ( pgdir = ( pde_t* ) kalloc() ) == 0 )
	{
		return 0;
	}

	// Clear junk
	memset( pgdir, 0, PGSIZE );

	// ?
	if ( P2V( PHYSTOP ) > ( void* ) DEVSPACE )
	{
		panic( "setupkvm: PHYSTOP too high" );
	}

	// ?
	for ( k = kmap; k < &kmap[ NELEM( kmap ) ]; k += 1 )
	{
		if (
			mappages(

				pgdir,
				k->virt,                      // virtual start address
				k->phys_end - k->phys_start,  // size
				( uint ) k->phys_start,       // physical start address
				k->perm
			) < 0 )
		{
			freevm( pgdir );

			return 0;
		}
	}

	return pgdir;
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


	// switch to the process's page table...
	lcr3( V2P( p->pgdir ) );


	// ?
	popcli();
}


// _________________________________________________________________________________

// Load the initcode into address 0 of pgdir.
// sz must be less than a page.
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
		PGSIZE,        // size
		V2P( mem ),    // physical start address
		PTE_W | PTE_U
	);

	// Copy the initcode binary to the page
	memmove( mem, init, sz );
}


// _________________________________________________________________________________

// Load a program segment into pgdir. addr must be page-aligned
// and the pages from addr to addr+sz must already be mapped.
int loaduvm ( pde_t* pgdir, char* addr, struct inode* ip, uint offset, uint sz )
{
	uint   i,
	       pa,
	       n;
	pte_t* pte;

	if ( ( uint ) addr % PGSIZE != 0 )
	{
		panic( "loaduvm: addr must be page aligned" );
	}

	for ( i = 0; i < sz; i += PGSIZE )
	{
		// Get PTE of ?
		if ( ( pte = walkpgdir( pgdir, addr + i, 0 ) ) == 0 )
		{
			panic( "loaduvm: address should exist" );
		}

		// Get physical address of ?
		pa = PTE_ADDR( *pte );


		//
		if ( sz - i < PGSIZE )
		{
			n = sz - i;
		}
		else
		{
			n = PGSIZE;
		}


		// Read from file...
		if ( readi( ip, P2V( pa ), offset + i, n ) != n )
		{
			return - 1;
		}
	}

	return 0;
}

// Allocate page tables and physical memory to grow process from oldsz to
// newsz, which need not be page aligned. Returns new size or 0 on error.
int allocuvm ( pde_t* pgdir, uint oldsz, uint newsz )
{
	char* mem;
	uint  a;

	if ( newsz >= KERNBASE )
	{
		return 0;
	}

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
		/* Security and clean garbage...
		   Also of minor note, C assumes that unitialized statics (BSS section)
		   have a value of zero...
		*/
		memset( mem, 0, PGSIZE );

		if ( mappages( pgdir, ( char* ) a, PGSIZE, V2P( mem ), PTE_W | PTE_U ) < 0 )
		{
			cprintf( "allocuvm: out of memory (2)\n" );

			deallocuvm( pgdir, newsz, oldsz );

			kfree( mem );

			return 0;
		}
	}

	return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz. oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz. oldsz can be larger than the actual
// process size. Returns the new process size.
int deallocuvm ( pde_t* pgdir, uint oldsz, uint newsz )
{
	pte_t* pte;
	uint   a,
	       pa;

	if ( newsz >= oldsz )
	{
		return oldsz;
	}

	a = PGROUNDUP( newsz );

	for ( ; a  < oldsz; a += PGSIZE )
	{
		pte = walkpgdir( pgdir, ( char* ) a, 0 );

		if ( ! pte )
		{
			a = PGADDR( PDX( a ) + 1, 0, 0 ) - PGSIZE;
		}
		else if ( ( *pte & PTE_P ) != 0 )
		{
			pa = PTE_ADDR( *pte );

			if ( pa == 0 )
			{
				panic( "kfree" );
			}

			char* v = P2V( pa );

			kfree( v );

			*pte = 0;
		}
	}

	return newsz;
}

// Free a page table and all the physical memory pages
// in the user part.
void freevm ( pde_t* pgdir )
{
	uint i;

	if ( pgdir == 0 )
	{
		panic( "freevm: no pgdir" );
	}

	deallocuvm( pgdir, KERNBASE, 0 );

	for( i = 0; i < NPDENTRIES; i += 1 )
	{
		if ( pgdir[ i ] & PTE_P )
		{
			char* v = P2V( PTE_ADDR( pgdir[ i ] ) );

			kfree( v );
		}
	}

	kfree( ( char* )pgdir );
}

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

// Given a parent process's page table, create a copy
// of it for a child.
pde_t* copyuvm ( pde_t* pgdir, uint sz )
{
	pde_t* d;
	pte_t* pte;
	uint   pa,
	       i,
	       flags;
	char*  mem;

	if ( ( d = setupkvm() ) == 0 )
	{
		return 0;
	}

	for ( i = 0; i < sz; i += PGSIZE )
	{
		if ( ( pte = walkpgdir( pgdir, ( void* ) i, 0 ) ) == 0 )
		{
			panic( "copyuvm: pte should exist" );
		}

		if ( ! ( *pte & PTE_P ) )
		{
			panic( "copyuvm: page not present" );
		}

		pa    = PTE_ADDR( *pte );
		flags = PTE_FLAGS( *pte );

		if ( ( mem = kalloc() ) == 0 )
		{
			goto bad;
		}

		memmove( mem, ( char* ) P2V( pa ), PGSIZE );

		if ( mappages( d, ( void* ) i, PGSIZE, V2P( mem ), flags ) < 0 )
		{
			kfree( mem );

			goto bad;
		}
	}

	return d;

bad:

	freevm( d );

	return 0;
}


// _________________________________________________________________________________

// Map user virtual address to kernel address.
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

// Copy len bytes from p to user address va in page table pgdir.
// Most useful when pgdir is not the current page table.
// uva2ka ensures this only works for PTE_U pages.
// Used by exec to copy arguments to the stack...
int copyout ( pde_t* pgdir, uint va, void* p, uint len )
{
	char* buf;
	char* pa0;
	uint  n,
	      va0;

	buf = ( char* ) p;

	while ( len > 0 )
	{
		va0 = ( uint ) PGROUNDDOWN( va );

		pa0 = uva2ka( pgdir, ( char* ) va0 );

		if ( pa0 == 0 )
		{
			return - 1;
		}

		n = PGSIZE - ( va - va0 );

		if ( n > len )
		{
			n = len;
		}

		memmove( pa0 + ( va - va0 ), buf, n );

		len -= n;
		buf += n;

		va = va0 + PGSIZE;
	}

	return 0;
}
