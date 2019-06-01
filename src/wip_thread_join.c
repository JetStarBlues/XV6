// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.

// https://youtu.be/uMXGjayZ9DE

// int wait ( void )
int join ( void )
{
	struct proc *p;
	int          havekids,
	             pid;
	struct proc *curproc = myproc();

	acquire( &ptable.lock );

	for ( ;; )
	{
		// Scan through table looking for exited children.
		havekids = 0;

		for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
		{
			if ( p->parent != curproc )
			{
				continue;
			}

			havekids = 1;

			// Found one
			if ( p->state == ZOMBIE )
			{
				pid = p->pid;

				// kfree( p->kstack );

				p->kstack = 0;

				// freevm( p->pgdir );

				p->pid       = 0;
				p->parent    = 0;
				p->name[ 0 ] = 0;
				p->killed    = 0;
				p->state     = UNUSED;

				release( &ptable.lock );

				return pid;
			}
		}

		// No point waiting if we don't have any children.
		if ( ! havekids || curproc->killed )
		{
			release( &ptable.lock );

			return - 1;
		}

		// Wait for children to exit. (See wakeup1 call in proc_exit.)
		sleep( curproc, &ptable.lock );  //DOC: wait-sleep
	}
}
