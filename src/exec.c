/*
	             0xFFFF_FFFF ->  --------------------
	                             memory mapped
	                             IO devices 
	  (DEVSPACE) 0xFE00_0000 ->  --------------------
	                             ...
	                             free memory
	                             ...
	             0x8040_0000 ->  --------------------  <-
	                             kernel static data      |
	                             --------------------    |
	                             kernel text             |
	             0x8010_0000 ->  --------------------    | kernel
	                             ?                       |
	             0x8000_7E00 ->  --------------------    |
	                             boot sector             |
	             0x8000_7C00 ->  --------------------    |
	                             ?                       |
	  (KERNBASE) 0x8000_0000 ->  --------------------  <-
	                             ...                     |
	                             --------------------    |
	                             user heap               |
	                             --------------------    |
	              (PAGESIZE)     user stack              | user
	                             --------------------    |
	              (PAGESIZE)     guard page              |
	                             --------------------    |
	                             program static data     |
	                             --------------------    |
	                             program text            |
	             0x0000_0000 ->  --------------------  <-
*/

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

/* exec replaces the memory and registers? of the current
   process with a new program.
   It leaves the file descriptors, process id, and
   parent process unchanged.
*/
int exec ( char* path, char* argv [] )
{
	char* s;
	char* last;
	int   i,
	      off;
	uint  argc,
	      sz,
	      sp,
	      ustack [ 3 + MAXARG + 1 ];

	struct elfhdr   elf;
	struct inode*   ip;
	struct proghdr  ph;
	pde_t*          pgdir;
	pde_t*          oldpgdir;
	struct proc*    curproc;


	//
	curproc = myproc();
	pgdir   = 0;
	sz      = 0;


	// Open ELF file
	begin_op();

	if ( ( ip = namei( path ) ) == 0 )
	{
		end_op();

		cprintf( "exec: fail\n" );

		return - 1;
	}

	ilock( ip );


	// Check ELF header
	if ( readi( ip, ( char* ) &elf, 0, sizeof( elf ) ) != sizeof( elf ) )
	{
		goto bad;
	}
	if ( elf.magic != ELF_MAGIC )
	{
		goto bad;
	}


	// Allocate a ??
	if ( ( pgdir = setupkvm() ) == 0 )
	{
		goto bad;
	}


	// Load program into memory

	// For each program section header
	for ( i = 0, off = elf.phoff; i < elf.phnum; i += 1, off += sizeof( ph ) )
	{
		// Read the ph
		if ( readi( ip, ( char* ) &ph, off, sizeof( ph ) ) != sizeof( ph ) )
		{
			goto bad;
		}

		// Ignore other types
		if ( ph.type != ELF_PROG_LOAD )
		{
			continue;
		}

		// We expect ph.memsz >= ph.filesz
		/*
		   stackoverflow.com/a/31011428
		    . The BSS section contains uninitialized data.
		    . As such it is wasteful to include it in the ELF file (ph.filesz),
		      and instead it is allocated space when the ELF is loaded into
		      memory (ph.memsz)

		   The BSS section starts at ph.filesz and ends at ph.memsz
		*/
		if ( ph.filesz > ph.memsz )
		{
			goto bad;
		}

		// Measure against kernel access...
		/* Check whether the sum overflows a 32bit integer. (See p.36).
		   'newsz' argument to allocuvm is (ph.vaddr + ph.memsz)
		   If ph.vaddr points to kernel,
		   and ph.memsz is large enough that newsz overflows to
		   a sum that is small enough to pass the
		   (newsz >= KERNBASE) check in allocuvm;
		   Then the subsequent call to loaduvm wil copy data from
		   the ELF binary into the kernel
		*/
		if ( ph.vaddr + ph.memsz < ph.vaddr )
		{
			goto bad;
		}

		// Allocate space in memory
		if ( ( sz = allocuvm( pgdir, sz, ph.vaddr + ph.memsz ) ) == 0 )
		{
			goto bad;
		}

		// Not page aligned
		if ( ph.vaddr % PGSIZE != 0 )
		{
			goto bad;
		}

		/*// JK debug...
		cprintf( "---\n" );
		cprintf( "Hi there!\n" );
		cprintf( path ); cprintf( "\n" );
		cprintf( "ph.vaddr  : %x\n", ph.vaddr );
		cprintf( "ph.off    : %x\n", ph.off );
		cprintf( "ph.filesz : %x\n", ph.filesz );
		cprintf( "---\n\n" ); */

		// Load file contents into memory
		/* ELF program sections:
		      .text
		      .rodata
		      .eh_frame
		      .data
		*/
		if ( loaduvm( pgdir, ( char* ) ph.vaddr, ip, ph.off, ph.filesz ) < 0 )
		{
			goto bad;
		}
	}

	iunlockput( ip );

	end_op();

	ip = 0;


	/* Allocate two pages at the next page boundary.
	   Make the first inaccessible.
	   Use the second as the user stack.
	*/
	/* With the inaccessible page, programs that try to use more than
	   the available stack will page fault.
	   Also when arguments to exec are too large, the 'copyout' function
	   used by exec to copy arguments to the stack will notice that
	   the destination page is inaccessible and return an error.
	*/
	/* Stack grows toward code? but should hit guard page first... ?
	*/
	sz = PGROUNDUP( sz );

	if ( ( sz = allocuvm( pgdir, sz, sz + 2 * PGSIZE ) ) == 0 )
	{
		goto bad;
	}

	clearpteu( pgdir, ( char* ) ( sz - 2 * PGSIZE ) );

	sp = sz;  // set stack pointer

	// cprintf( "sp (%s): %d\n", path, sp );


	/* Push argument strings received (from argv) onto stack,
	   and prepare rest of stack in ustack.
	*/
	/* Ex.
	       ls -p /usr/include

	       argc -> 3

	       argv -> argptr -> ls\0            // argv[0] convention
	               argptr -> -p\0
	               argptr -> /usr/include\0
	               0                         // argv[] is null terminated
	*/
	/* ustack prepared as follows (TODO, confirm):

	       ustack -> returnAddress
	                 argc
	                 argvptr                  // hmmm... seems redundant...
	                 argptr -> ls\0
	                 argptr -> -p\0
	                 argptr -> /usr/include\0
	                 0

	   stack prepared as follows:

	                 \0
	                 s
	              -> l
	                 \0
	                 p
	              -> -
	                 \0
	                 ...
	                 r
	                 s
	                 u
	              -> /
	                 0
	                 argptr -> /usr/include\0
	                 argptr -> -p\0
	         argv -> argptr -> ls\0
	                 argvptr
	                 argc
	       ustack -> returnAddress
	           sp -> ...
	*/
	for ( argc = 0; argv[ argc ]; argc += 1 )
	{
		if ( argc >= MAXARG )
		{
			goto bad;
		}

		// Push string
		sp -= strlen( argv[ argc ] ) + 1;

		sp &= ~ 3;  // multiples of 4?

		if ( copyout(

				pgdir,
				sp,                         // to
				argv[ argc ],               // from
				strlen( argv[ argc ] ) + 1  // len

			) < 0 )
		{
			goto bad;
		}

		// Save pointer to pushed string ( argv[ argc ] )
		ustack[ 3 + argc ] = sp;
	}

	ustack[ 3 + argc ] = 0;  // mark end of arguments

	ustack[ 0 ] = 0xffffffff;             // fake return PC (don't expect exec to return)
	ustack[ 1 ] = argc;                   // argc
	ustack[ 2 ] = sp - ( argc + 1 ) * 4;  // argv pointer


	// Push ustack
	sp -= ( 3 + argc + 1 ) * 4;  // size of ustack

	if ( copyout(

			pgdir,
			sp,                   // to
			ustack,               // from
			( 3 + argc + 1 ) * 4  // len

		) < 0 )
	{
		goto bad;
	}


	// Save program name for debugging.
	for ( last = s = path; *s; s += 1 )
	{
		if ( *s == '/' )
		{
			last = s + 1;
		}
	}

	safestrcpy( curproc->name, last, sizeof( curproc->name ) );


	// Commit to the new user image.
	/* exec must wait to free the old image until it is will succeed
	   because if the old image is gone, it cannot return an error to it.
	*/
	oldpgdir = curproc->pgdir;

	curproc->pgdir   = pgdir;
	curproc->sz      = sz;         // at this point, points to heap_base/stack_end ??
	curproc->tf->eip = elf.entry;  // main (see Makefile)
	curproc->tf->esp = sp;

	switchuvm( curproc );

	freevm( oldpgdir );

	return 0;

 bad:

	if ( pgdir )
	{
		freevm( pgdir );
	}

	if ( ip )
	{
		iunlockput( ip );

		end_op();
	}

	return - 1;
}
