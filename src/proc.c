/*
Context switching

	scheduler calls:

		swtch( &( c->scheduler ), p->context )

		. from kernel to user

	sched calls:

		swtch( &p->context, mycpu()->scheduler )

		. from user to kernel

		. sched is called by
			. yield
			. sleep
			. exit
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

struct
{
	struct spinlock lock;
	struct proc     proc [ NPROC ];

} ptable;

static struct proc* initproc;
int                 nextpid = 1;

extern void forkret ( void );
extern void trapret ( void );

static void wakeup1 ( void* chan );


// _________________________________________________________________________________

void pinit ( void )
{
	initlock( &ptable.lock, "ptable" );
}

// Must be called with interrupts disabled
int cpuid ()
{
	return mycpu() - cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu* mycpu ( void )
{
	int apicid, i;

	if ( readeflags() & FL_IF )
	{
		panic( "mycpu called with interrupts enabled\n" );
	}

	apicid = lapicid();

	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for ( i = 0; i < ncpu; ++i )
	{
		if ( cpus[ i ].apicid == apicid )
		{
			return &cpus[ i ];
		}
	}

	panic( "unknown apicid\n" );
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc* myproc ( void )
{
	struct cpu*  c;
	struct proc* p;

	pushcli();

	c = mycpu();

	p = c->proc;

	popcli();

	return p;
}


// _________________________________________________________________________________

/* Look in the process table for an UNUSED proc.
   If found, change state to EMBRYO and initialize a kernel stack.
   Otherwise return 0.

   allocproc is designed to be ued both by userinit (when creating the
   first process) and fork
*/
static struct proc* allocproc ( void )
{
	struct proc* p;
	char*        sp;

	acquire( &ptable.lock );

	// Scan process table for an available slot
	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->state == UNUSED )
		{
			goto found;
		}
	}

	release( &ptable.lock );

	return 0;

found:

	p->state = EMBRYO;   // mark as used
	p->pid   = nextpid;  // give unique PID

	nextpid += 1;

	release( &ptable.lock );


	// Allocate kernel stack
	if( ( p->kstack = kalloc() ) == 0 )
	{
		p->state = UNUSED;

		return 0;
	}

	sp = p->kstack + KSTACKSIZE;


	/* Prepare the kernel stack so that:
	   . it looks like we entered it through an interrupt ??, and
	   . it will "return" to user code
	*/

	// Leave room for trap frame
	sp -= sizeof *p->tf;

	p->tf = ( struct trapframe* ) sp;


	/* Setup program counter values that will cause the new
	   process to first execute forkret and then trapret
	*/

	// Trapret restores user registers from values stored in the
	// kernel stack ("struct trapframe", p->tf)
	sp -= 4;

	*( uint* ) sp = ( uint ) trapret;


	// The process will start executing with the register values found
	// in p->context
	sp -= sizeof *p->context;

	p->context = ( struct context* ) sp;

	memset( p->context, 0, sizeof *p->context );  // set all registers in context to 0

	p->context->eip = ( uint ) forkret;


	/* When all is said and done, the kernel stack looks like this:

	           ->  ---------------  <-- p->kstack + KSTACKSIZE
	          |    ss
	          |    ---------------
	          |    esp
	trapframe |    ---------------
	          |    ...
	          |    ---------------
	          |    edi
	           ->  ---------------  <-- p->tf
	               trapret
	           ->  ---------------  <-- p->tf - 4; address forkret will return to (trapret)
	          |    eip (forkret)
	          |    ---------------  <-- the process will start executing at p->context->eip (forkret)
	          |    ebp (0)
	          |    ---------------
	  context |    ebx (0)
	          |    ---------------
	          |    esi (0)
	          |    ---------------
	          |    edi (0)
	           ->  ---------------  <-- p->context
	               ...
	               empty
	               ...
	               ---------------  <-- p->kstack
	*/

	//
	return p;
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc ( int n )
{
	uint sz;

	struct proc* curproc = myproc();

	sz = curproc->sz;

	// Allocate n pages and add mappings
	if ( n > 0 )
	{
		if ( ( sz = allocuvm( curproc->pgdir, sz, sz + n ) ) == 0 )
		{
			return - 1;
		}
	}

	// Deallocate abs(n) pages and remove mappings
	else if ( n < 0 )
	{
		if ( ( sz = deallocuvm( curproc->pgdir, sz, sz + n ) ) == 0 )
		{
			return - 1;
		}
	}

	curproc->sz = sz;

	switchuvm( curproc );

	return 0;
}


// _________________________________________________________________________________

// Set up first user process.
void userinit ( void )
{
	// extern char _binary_initcode_start[], _binary_initcode_size[];
	extern char _binary_img_initcode_start [],
	            _binary_img_initcode_size  [];  // JK, new path

	struct proc* p;


	// Allocate an UNUSED process from the process table, and allocate
	// and initialize its kernel stack
	p = allocproc();

	initproc = p;


	// Create a page table for the process
	// The page table will first only hold mappings for memory used
	// by the kernel...
	if ( ( p->pgdir = setupkvm() ) == 0 )
	{
		panic( "userinit: out of memory?" );
	}


	// Copy initcode's binary into the process's user-space memory
	inituvm(

		p->pgdir,
		// _binary_initcode_start,
		// ( int ) _binary_initcode_size
		_binary_img_initcode_start,
		( int ) _binary_img_initcode_size
	);

	// initcode's binary is expected to be equal to or less than one page in size
	p->sz = PGSIZE;


	/* Write values in the new process's trapframe that make it seem
	   like the process entered the kernel via an interrupt

	   Set tf->eip to zero so that the process starts executing at user-space
	   location zero on reti by trapret
	*/
	memset( p->tf, 0, sizeof( *p->tf ) );

	p->tf->cs     = ( SEG_UCODE << 3 ) | DPL_USER;
	p->tf->ds     = ( SEG_UDATA << 3 ) | DPL_USER;
	p->tf->es     = p->tf->ds;
	p->tf->ss     = p->tf->ds;

	p->tf->eflags = FL_IF;   // enable hardware interrupts ??
	p->tf->esp    = PGSIZE;  // the process's largest valid virtual address. Why ??
	p->tf->eip    = 0;       // beginning (entry point) of initcode.S


	// Set process name. For debugging
	safestrcpy( p->name, "initcode", sizeof( p->name ) );

	// Set cwd to root directory...
	p->cwd = namei( "/" );


	// This assignment to p->state lets other cores
	// run this process. The acquire forces the above
	// writes to be visible, and the lock is also needed
	// because the assignment might not be atomic.
	acquire( &ptable.lock );

	p->state = RUNNABLE;  // Mark the process as avaialble for scheduling

	release( &ptable.lock );
}


// _________________________________________________________________________________

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int fork ( void )
{
	int          i,
	             pid;
	struct proc* np;
	struct proc* curproc = myproc();

	// Allocate process.
	if ( ( np = allocproc() ) == 0 )
	{
		return - 1;
	}

	// Copy process state from proc.
	if ( ( np->pgdir = copyuvm( curproc->pgdir, curproc->sz ) ) == 0 )
	{
		kfree( np->kstack );

		np->kstack = 0;
		np->state  = UNUSED;

		return - 1;
	}

	np->parent = curproc;
	np->sz     = curproc->sz;

	// Use same trapframe as parent
	/* This also has effect that the child will resume execution at the
	   same point (tf->eip) as the parent.
	   This corresponds to the point after the parent's call to fork.
	*/
	*np->tf    = *curproc->tf;

	// Clear %eax so that fork returns 0 in the child.
	np->tf->eax = 0;

	// Copy file descriptors
	for ( i = 0; i < NOFILE; i += 1 )
	{
		if ( curproc->ofile[ i ] )
		{
			np->ofile[ i ] = filedup( curproc->ofile[ i ] );
		}
	}

	np->cwd = idup( curproc->cwd );

	safestrcpy( np->name, curproc->name, sizeof( curproc->name ) );

	pid = np->pid;

	acquire( &ptable.lock );

	np->state = RUNNABLE;

	release( &ptable.lock );

	return pid;
}

// Exit the current process. Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit ( void )
{
	struct proc* curproc = myproc();
	struct proc* p;
	int          fd;

	if ( curproc == initproc )
	{
		panic( "init exiting" );
	}

	// Close all open files.
	for ( fd = 0; fd < NOFILE; fd += 1 )
	{
		if ( curproc->ofile[ fd ] )
		{
			fileclose( curproc->ofile[ fd ] );

			curproc->ofile[ fd ] = 0;
		}
	}

	begin_op();

	iput( curproc->cwd );

	end_op();

	curproc->cwd = 0;

	acquire( &ptable.lock );

	// Parent might be sleeping in wait()
	wakeup1( curproc->parent );

	// Pass abandoned children to init.
	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->parent == curproc )
		{
			p->parent = initproc;

			if ( p->state == ZOMBIE )
			{
				wakeup1( initproc );
			}
		}
	}

	// Jump into the scheduler, never to return.
	curproc->state = ZOMBIE;

	sched();

	panic( "zombie exit" );
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int wait ( void )
{
	struct proc* p;
	int          havekids,
	             pid;
	struct proc* curproc = myproc();

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

				kfree( p->kstack );

				p->kstack = 0;

				freevm( p->pgdir );

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


// _________________________________________________________________________________

// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns. It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void scheduler ( void )
{
	struct proc* p;
	struct cpu*  c = mycpu();

	c->proc = 0;

	for ( ;; )
	{
		// Enable interrupts on this processor.
		sti();

		// Loop over process table looking for process to run.
		acquire( &ptable.lock );

		for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
		{
			if ( p->state != RUNNABLE )
			{
				continue;
			}


			/* Switch to chosen process. It is the process's job
			   to release ptable.lock and then reacquire it
			   before jumping back to us.
			*/
			c->proc = p;

			// switch to the process's page table...
			switchuvm( p );

			p->state = RUNNING;

			/* Context switch into the process's kernel thread...

			   The current context is not a process. It is is a special
			   per-cpu scheduler context (cpu->scheduler)...

			   swtch will save the current registers to cpu->scheduler
			   and restore those saved in p->context
			*/
			swtch( &( c->scheduler ), p->context );


			/* ...
			*/

			// switch to the kernel-only page table...
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}

		release( &ptable.lock );
	}
}

// Enter scheduler. Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void sched ( void )
{
	int          intena;
	struct proc* p = myproc();

	if ( ! holding( &ptable.lock ) )
	{
		panic( "sched ptable.lock" );
	}

	if ( mycpu()->ncli != 1 )
	{
		panic( "sched locks" );
	}

	if ( p->state == RUNNING )
	{
		panic( "sched running" );
	}

	if ( readeflags() & FL_IF )
	{
		panic( "sched interruptible" );
	}

	intena = mycpu()->intena;

	swtch( &p->context, mycpu()->scheduler );

	mycpu()->intena = intena;
}

// A fork child's very first scheduling by scheduler()
// will swtch here. "Return" to user space.
void forkret ( void )
{
	static int first = 1;

	// Still holding ptable.lock from scheduler.
	release( &ptable.lock );

	if ( first )
	{
		// Some initialization functions must be run in the context
		// of a regular process (e.g., they call sleep), and thus cannot
		// be run from main().
		first = 0;

		iinit( ROOTDEV );

		initlog( ROOTDEV );
	}

	// Return to "caller", actually trapret (see allocproc).
}


// _________________________________________________________________________________

// Give up the CPU for one scheduling round.
void yield ( void )
{
	acquire( &ptable.lock );  //DOC: yieldlock

	myproc()->state = RUNNABLE;

	sched();

	release( &ptable.lock );
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void sleep ( void* chan, struct spinlock* lk )
{
	struct proc* p = myproc();

	if ( p == 0 )
	{
		panic( "sleep" );
	}

	if ( lk == 0 )
	{
		panic( "sleep without lk" );
	}

	// Must acquire ptable.lock in order to
	// change p->state and then call sched.
	// Once we hold ptable.lock, we can be
	// guaranteed that we won't miss any wakeup
	// (wakeup runs with ptable.lock locked),
	// so it's okay to release lk.
	if ( lk != &ptable.lock )  //DOC: sleeplock0
	{
		acquire( &ptable.lock );  //DOC: sleeplock1

		release( lk );
	}

	// Go to sleep.
	p->chan  = chan;
	p->state = SLEEPING;

	sched();

	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if ( lk != &ptable.lock )  //DOC: sleeplock2
	{
		release( &ptable.lock );

		acquire( lk );
	}
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void wakeup1 ( void *chan )
{
	struct proc* p;

	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->state == SLEEPING && p->chan == chan )
		{
			p->state = RUNNABLE;
		}
	}
}

// Wake up all processes sleeping on chan.
void wakeup ( void* chan )
{
	acquire( &ptable.lock );

	wakeup1( chan );

	release( &ptable.lock );
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space ( see trap in trap.c ).
int kill ( int pid )
{
	struct proc* p;

	acquire( &ptable.lock );

	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->pid == pid )
		{
			p->killed = 1;

			// Wake process from sleep if necessary.
			if( p->state == SLEEPING )
			{
				p->state = RUNNABLE;
			}

			release( &ptable.lock );

			return 0;
		}
	}

	release( &ptable.lock );

	return - 1;
}


// _________________________________________________________________________________

// Print a process listing to console. For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void procdump ( void )
{
	static char* states [] = {

		[ UNUSED   ] "unused  ",
		[ EMBRYO   ] "embryo  ",
		[ SLEEPING ] "sleeping",
		[ RUNNABLE ] "runnable",
		[ RUNNING  ] "running ",
		[ ZOMBIE   ] "zombie  "
	};

	struct proc* p;
	char*        state;
	int          i;
	uint         pc [ 10 ];

	cprintf( "\nprocdump:\n" );
	cprintf( "pid | state | name\n" );
	cprintf( "------------------\n\n" );

	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->state == UNUSED )
		{
			continue;
		}

		if ( p->state >= 0 && p->state < NELEM( states ) && states[ p->state ] )
		{
			state = states[ p->state ];
		}
		else
		{
			state = "???";
		}

		cprintf( "%d | %s | %s\n", p->pid, state, p->name );

		if ( p->state == SLEEPING )
		{
			getcallerpcs( ( uint* )p->context->ebp + 2, pc );

			cprintf( "call stack:\n" );

			for ( i = 0; i < 10 && pc[ i ] != 0; i += 1 )
			{
				cprintf( "    %p\n", pc[ i ] );
			}

			cprintf( "\n" );
		}
	}

	cprintf( "\n" );
}
