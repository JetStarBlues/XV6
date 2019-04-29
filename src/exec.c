#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int exec ( char *path, char **argv )
{
	char *s,
	     *last;
	int   i,
	      off;
	uint  argc,
	      sz,
	      sp,
	      ustack [ 3 + MAXARG + 1 ];

	struct elfhdr   elf;
	struct inode   *ip;
	struct proghdr  ph;
	pde_t          *pgdir,
	               *oldpgdir;
	struct proc    *curproc = myproc();

	begin_op();

	if ( ( ip = namei( path ) ) == 0 )
	{
		end_op();

		cprintf( "exec: fail\n" );

		return - 1;
	}

	ilock( ip );

	pgdir = 0;

	// Check ELF header
	if ( readi( ip, ( char* )&elf, 0, sizeof( elf ) ) != sizeof( elf ) )
	{
		goto bad;
	}
	if ( elf.magic != ELF_MAGIC )
	{
		goto bad;
	}

	if ( ( pgdir = setupkvm() ) == 0 )
	{
		goto bad;
	}

	// Load program into memory.
	sz = 0;

	// For each program section header
	for ( i = 0, off = elf.phoff; i < elf.phnum; i += 1, off += sizeof( ph ) )
	{
		// Read the ph
		if ( readi( ip, ( char* )&ph, off, sizeof( ph ) ) != sizeof( ph ) )
		{
			goto bad;
		}

		// Ignore other types
		if ( ph.type != ELF_PROG_LOAD )
		{
			continue;
		}

		// We expect ph.filesz <= ph.memsz
		if ( ph.filesz > ph.memsz )
		{
			goto bad;
		}

		// Measure against kernel access...
		/* Check whether the sum overflows a 32bit integer.
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

		// Allocate space
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
		if ( loaduvm( pgdir, ( char* )ph.vaddr, ip, ph.off, ph.filesz ) < 0 )
		{
			goto bad;
		}
	}

	iunlockput( ip );

	end_op();

	ip = 0;

	// Allocate two pages at the next page boundary.
	// Make the first inaccessible. Use the second as the user stack.
	sz = PGROUNDUP( sz );

	if ( ( sz = allocuvm( pgdir, sz, sz + 2 * PGSIZE ) ) == 0 )
	{
		goto bad;
	}

	clearpteu( pgdir, ( char* )( sz - 2 * PGSIZE ) );

	sp = sz;

	// Push argument strings, prepare rest of stack in ustack.
	for ( argc = 0; argv[ argc ]; argc += 1 )
	{
		if ( argc >= MAXARG )
		{
			goto bad;
		}

		// ? why & ~ 3? multiples of 4?
		sp = ( sp - ( strlen( argv[ argc ] ) + 1 ) ) & ~ 3;

		if ( copyout(

				pgdir,
				sp,
				argv[ argc ],
				strlen( argv[ argc ] ) + 1
			) < 0 )
		{
			goto bad;
		}

		ustack[ 3 + argc ] = sp;
	}

	ustack[ 3 + argc ] = 0;  // mark end of arguments

	ustack[ 0 ] = 0xffffffff;             // fake return PC
	ustack[ 1 ] = argc;
	ustack[ 2 ] = sp - ( argc + 1 ) * 4;  // argv pointer

	sp -= ( 3 + argc + 1 ) * 4;

	if ( copyout(

			pgdir,
			sp,
			ustack,
			( 3 + argc + 1 ) * 4
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

	// Commit to the user image.
	oldpgdir = curproc->pgdir;

	curproc->pgdir   = pgdir;
	curproc->sz      = sz;
	curproc->tf->eip = elf.entry;  // main
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
