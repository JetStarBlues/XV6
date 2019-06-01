// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.

// https://youtu.be/uMXGjayZ9DE

// int fork ( void )
int clone ( void )
{
	int          i,
	             pid;
	struct proc *np;
	struct proc *curproc = myproc();

	// Allocate process.
	if ( ( np = allocproc() ) == 0 )
	{
		return - 1;
	}
	// TODO - each new thread will have its own entry in the process table...?

	// Copy process state from proc.
	/*if ( ( np->pgdir = copyuvm( curproc->pgdir, curproc->sz ) ) == 0 )
	{
		kfree( np->kstack );

		np->kstack = 0;
		np->state  = UNUSED;

		return - 1;
	}*/
	// TODO - have both point to same page directory...

	np->sz     = curproc->sz;
	np->parent = curproc;
	*np->tf    = *curproc->tf;

	// Clear %eax so that fork returns 0 in the child.
	// np->tf->eax = 0;
	// TODO - change to passed in fx pointer

	// TODO - update stack pointer in trapframe (tf->esp, tf->ebp)
	/* e.g.
		stack (clone arg) = 0x9000
		curproc->tf->esp  = 0x4203
		curproc->tf->ebp  = 0x4212

		np->tf->esp = 0x9203
		np->tf->ebp = 0x9212

			one page is 4096 or 0x1000 bytes
			new page is 9...
			old page is 4...
			so new page + offset ???
	*/
	// TODO - copy contents of stack to new thread
	/* e.g.
		copy page 4 to page 9...
	*/

	// Copy file descriptors
	for ( i = 0; i < NOFILE; i += 1 )
	{
		if ( curproc->ofile[ i ] )
		{
			np->ofile[ i ] = filedup( curproc->ofile[ i ] );
		}
	}
	/*
		Apparently in Linux, all threads and parent share fd table (ofile)
		(instead of each unique)
	*/

	np->cwd = idup( curproc->cwd );

	safestrcpy( np->name, curproc->name, sizeof( curproc->name ) );

	pid = np->pid;

	acquire( &ptable.lock );

	np->state = RUNNABLE;

	release( &ptable.lock );

	return pid;
}
