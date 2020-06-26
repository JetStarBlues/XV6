/*
Context switching

	sched calls:

		swtch( &p->context, mycpu()->scheduler )

		. from user to kernel

		. sched is called by
			. yield
			. sleep
			. exit

	scheduler calls:

		swtch( &( c->scheduler ), p->context )

		. from kernel to user
			. exit

	xv6 multiplexes in two situations:
		. sleep and wakeup mechanism - when a process waits for an event
		. periodically (on timer interrupt) with yield

	Multiplexing (from user space) steps:
		. User to kernel transition (via INT instruction)
		  into the current process's kernel thread
		. Context switch to the CPU's scheduler thread
		  (via alltraps                                 >
		       trap                                     >
		       yield                                    >
		       sched                                    >
		       swtch( &p->context, mycpu()->scheduler ) >
		       switchkvm
		   )
		. Context switch to the new process's kernel thread
		  (via switchuvm                              >
		       swtch( &( c->scheduler ), p->context ) >
		       trapret
		  )
		. Kernel to user transition (via IRET instruction)
		  into the new process's user thread

	xv6 uses two context switches because the scheduler runs on its
	own stack in order to simplify cleaning up user processes... ??
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
	/* This lock is used for ... ??
	*/
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
	int apicid;
	int i;

	if ( readeflags() & FL_IF )
	{
		panic( "mycpu: called with interrupts enabled\n" );
	}

	apicid = lapicid();

	// APIC IDs are not guaranteed to be contiguous. Maybe we should have
	// a reverse map, or reserve a register to store &cpus[i].
	for ( i = 0; i < ncpu; i += 1 )
	{
		if ( cpus[ i ].apicid == apicid )
		{
			return &cpus[ i ];
		}
	}

	panic( "mycpu: unknown apicid\n" );
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

   allocproc is designed to be used both by userinit (when creating the
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

	p->state = EMBRYO;   // mark as used, but not ready to run yet
	p->pid   = nextpid;  // give unique PID

	nextpid += 1;

	release( &ptable.lock );


	// Allocate kernel stack
	p->kstack = kalloc();  // one page

	if ( p->kstack == 0 )
	{
		p->state = UNUSED;

		return 0;
	}

	sp = p->kstack + KSTACKSIZE;  // top of kernel stack


	/* Prepare the kernel stack so that:
	   . it looks like we entered it through an interrupt ??, and
	   . it will "return" to user code
	*/

	// Leave room for trap frame
	sp -= sizeof( struct trapframe );

	p->tf = ( struct trapframe* ) sp;


	/* Setup program counter values that will cause the new
	   process to first execute forkret and then trapret
	*/

	// Trapret restores user registers from values stored in the
	// kernel stack ("struct trapframe", p->tf)
	sp -= 4;

	*( ( uint* ) sp ) = ( uint ) trapret;


	// The process will start executing with the register values found
	// in p->context
	sp -= sizeof( struct context );

	p->context = ( struct context* ) sp;

	memset( p->context, 0, sizeof( struct context ) );  // set all registers in context to 0

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

// How does a pid of zero (child) get returned ??
int fork ( void )
{
	struct proc* curproc;
	struct proc* newproc;
	int          i,
	             pid;

	//
	curproc = myproc();

	// Allocate process
	newproc = allocproc();

	if ( newproc == 0 )
	{
		return - 1;
	}

	// Setup its page table as a copy of curproc's
	newproc->pgdir = copyuvm( curproc->pgdir, curproc->sz );

	if ( newproc->pgdir == 0 )
	{
		kfree( newproc->kstack );

		newproc->kstack = 0;
		newproc->state  = UNUSED;

		return - 1;
	}

	newproc->parent = curproc;
	newproc->sz     = curproc->sz;

	// Use same trapframe as parent
	/* This also has effect that the child will resume execution at the
	   same point (tf->eip) as the parent.
	   This corresponds to the point after the parent's call to fork.
	*/
	*( newproc->tf ) = *( curproc->tf );

	// Clear %eax so that fork returns 0 in the child.
	newproc->tf->eax = 0;

	// Copy file descriptors
	for ( i = 0; i < NOPENFILE_PROC; i += 1 )
	{
		if ( curproc->ofile[ i ] )
		{
			newproc->ofile[ i ] = filedup( curproc->ofile[ i ] );
		}
	}

	newproc->cwd = idup( curproc->cwd );

	safestrcpy( newproc->name, curproc->name, sizeof( curproc->name ) );

	pid = newproc->pid;  //

	acquire( &ptable.lock );

	newproc->state = RUNNABLE;

	release( &ptable.lock );

	return pid;
}


// _________________________________________________________________________________

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int growproc ( int n )
{
	struct proc* curproc;
	uint         sz;

	//
	curproc = myproc();

	sz = curproc->sz;

	// Allocate n pages and add mappings
	if ( n > 0 )
	{
		sz = allocuvm( curproc->pgdir, sz, sz + n );

		if ( sz == 0 )
		{
			return - 1;
		}
	}

	// Deallocate abs(n) pages and remove mappings
	else if ( n < 0 )
	{
		sz = deallocuvm( curproc->pgdir, sz, sz + n );

		if ( sz == 0 )
		{
			return - 1;
		}
	}

	curproc->sz = sz;

	switchuvm( curproc );

	return 0;
}


// _________________________________________________________________________________

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
/*  . When a child exits, it does not die immediately.
      Instead, it switches to the ZOMBIE state until its parent
      calls 'wait' to learn of the exit.
    . The parent is then responsible for freeing the memory
      associated with the child and preparing its "struct proc"
      for reuse.
    . If the parent exits before the child, the init process
      adopts the child and waits for it.
*/
int wait ( void )
{
	struct proc* curproc;
	struct proc* p;
	int          havekids,
	             pid;

	//
	curproc = myproc();

	//
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

			// Found one, clean up after it
			if ( p->state == ZOMBIE )
			{
				pid = p->pid;

				// Free associated memory
				/* The parent frees p->kstack and p->pgdir because the
				   child uses them one last time when running 'exit'
				*/
				kfree( p->kstack );

				freevm( p->pgdir );

				// Prepare the "struct proc" for reuse
				p->kstack    = 0;
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

		// Wait for children to exit. See wakeup1 call in exit.
		sleep( curproc, &ptable.lock );
	}
}

// Exit the current process. Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void exit ( void )
{
	struct proc* curproc;
	struct proc* p;
	int          fd;

	//
	curproc = myproc();

	//
	if ( curproc == initproc )
	{
		panic( "exit: init exiting" );
	}

	// Close all open files.
	for ( fd = 0; fd < NOPENFILE_PROC; fd += 1 )
	{
		if ( curproc->ofile[ fd ] )
		{
			fileclose( curproc->ofile[ fd ] );

			curproc->ofile[ fd ] = 0;
		}
	}

	// ??
	begin_op();

	iput( curproc->cwd );

	end_op();

	curproc->cwd = 0;


	acquire( &ptable.lock );

	// Wakeup parent. They might be sleeping in wait()
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

	panic( "exit: zombie exit" );
}

// Kill the process with the given pid
/* Lets one process request that another be terminated.

   Just sets p->killed, and if the process is sleeping,
   wakes it up.
   Eventually the process will enter or leave the kernel
   (ex. via system call, timer interrupt), at which point
   code in trap will call exit if p->killed is set.
*/
int kill ( int pid )
{
	struct proc* p;

	acquire( &ptable.lock );

	for ( p = ptable.proc; p < &ptable.proc[ NPROC ]; p += 1 )
	{
		if ( p->pid == pid )
		{
			p->killed = 1;

			// Wake process from sleep if necessary
			/* Potenially dangerous because the condition the
			   process is waiting on might not be true. However,
			   xv6 convention is to wrap sleep calls in a while-loop
			   that retests the condition after sleep returns.
			   Some calls to sleep test p->killed to abandon early.
			*/
			if ( p->state == SLEEPING )
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

// Per-CPU process scheduler.
/* Each CPU calls scheduler() after setting itself up.
   Scheduler never returns. It loops, doing:
    - choose a process to run
    - swtch to start running that process
    - eventually that process transfers control
        via swtch back to the scheduler.
*/
/* Each CPU has a seperate scheduler thread for use when
   it is executing the scheduler. (Instead of using the
   current process's kernel thread)

   Switching from one thread to another (for ex. from the
   current process's kernel thread to the CPU's scheduler
   thread) involves saving the current thread's registers, and
   restoring the new thread's registers (including %eip and %esp
   which specify what code is executing and which stack is used)

   The scheduler releases ptable.lock and explicitly enables
   interrupts once in each iteration of its outer loop.
   This is important for the case where there are multiple
   CPUs and one is idle (it has no RUNNABLE processes).
   If ptable.lock was continuously held, other CPUs cannot perform
   context switches as they cannot acquire ptable.lock.
   They also cannot mark a process as RUNNABLE so as to break the
   idling CPU out of its scheduling loop...
   Interrupts are periodically enabled because on an idling CPU,
   perharps some of the processes are actually waiting for IO (lapic...)

   Changing page tables while executing in the kernel works because
   all processes have identical mappings for kernel code and data.
*/
void scheduler ( void )
{
	struct proc* p;
	struct cpu*  c;

	//
	c = mycpu();

	c->proc = 0;

	//
	for ( ;; )
	{
		// Enable interrupts on this processor.
		sti();

		// Acquire process table lock
		acquire( &ptable.lock );

		// Loop over process table looking for process to run.
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

			// cprintf( "scheduler: switching to process %s\n", p->name );


			// Switch to the process's page table...
			switchuvm( p );

			p->state = RUNNING;

			/* Context switch into the process's kernel thread...

			   The current context is not a process. It is is a special
			   per-cpu scheduler context (cpu->scheduler)...

			   swtch will save the current registers to cpu->scheduler
			   and restore those saved in p->context

			   From there 'trapret' will execute as the process's
			   kernel thread returns from the 'trap' function that
			   led it here...
			*/
			swtch( &( c->scheduler ), p->context );


			// - - - - - - - - - - - - - - - - - - - - - - - - - -


			/* A call to 'sched' ends up here...
			   More specifically, the call to:
			     swtch( &p->context, mycpu()->scheduler )
			*/

			// Switch to the kernel-only page table...
			switchkvm();

			// Process is done running for now.
			// It should have changed its p->state before coming back.
			c->proc = 0;
		}

		// Release process table lock
		release( &ptable.lock );
	}
}

// Enter scheduler.
/* Must hold only ptable.lock and have changed proc->state.
   Saves and restores intena because intena is a property of this
   kernel thread, not this CPU.
   It should be proc->intena and proc->ncli, but that would
   break in the few places where a lock is held but there's no process
   (such as ??)
*/
/* When context switching, the thread that acquires the lock is
   not responsible for releasing it...
   The convention is broken because ... see p.63 & p.64
*/
void sched ( void )
{
	struct proc* p;
	int          intena;

	p = myproc();

	if ( ! holding( &ptable.lock ) )
	{
		panic( "sched: ptable.lock not held" );
	}

	if ( mycpu()->ncli != 1 )
	{
		// Only ptable.lock should be held ??
		panic( "sched: number of locks" );
	}

	if ( p->state == RUNNING )
	{
		panic( "sched: running" );
	}

	if ( readeflags() & FL_IF )
	{
		// Since a lock is held, the CPU should be running with interrupts disabled
		panic( "sched: interruptible" );
	}


	// Save the process's interrupt enable state
	intena = mycpu()->intena;

	/* Context switch into the scheduler's kernel thread...

	   swtch will save the current registers to p->context
	   and restore those saved in cpu->scheduler

	   From there, 'scheduler' will go on to schedule
	   another process to run
	*/
	swtch( &p->context, mycpu()->scheduler );


	// - - - - - - - - - - - - - - - - - - - - - - - - - -


	/* A call to:
	     swtch( &( c->scheduler ), p->context )
	   by the scheduler ends up here...
	*/

	// Restore the process's interrupt enable state
	mycpu()->intena = intena;
}

/* A fork child's very first scheduling by scheduler()
   will 'swtch' here.
   And then "return" to user-space via trapret.
*/
/* forkret exists to release the ptable.lock held
   by the scheduler. Otherwise, the new process could
   start at trapret
*/
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

		iinit( ROOTDEV );    // ...
		initlog( ROOTDEV );  // initialize log. Recover file system if necessary
	}

	// Return to "caller", actually trapret (see allocproc).
}


// _________________________________________________________________________________

// Give up the CPU for one scheduling round.
void yield ( void )
{
	acquire( &ptable.lock );

	myproc()->state = RUNNABLE;

	sched();

	release( &ptable.lock );
}


// _________________________________________________________________________________

/* Sleep and wakeup allow processes to intentionally interact.
   One process can sleep to wait for an event, and another can
   wake it up once the event has happened...
*/

/* Atomically release lock and sleeps on chan ??
   Reacquires lock when awakened.
*/
/* Used to:
    . implement 'sleep' system call
    . implement sleeplocks
    . wait on slow IO (disk, console)
    . wait on pipe ends...
    . wait on a child to exit

   Instead of wasting CPU cycles polling for the occurence
   of an event, a process can give up the CPU and sleep
   waiting for the event. Another process can then wake it up
   when the event occurs.
*/
/* Sleeps on the arbitrary value 'chan'.

   Caller passes lock to sleep so that 'sleep' can release
   the lock before sleeping, and reacquire it when
   it wakes up... p.68

   We need sleep to atomically release the lock and put
   the process to sleep... ??
   By holding ptable.lock, we ensure atomicity because fkdslfdf

   p.69 ...
*/
/* Sleep must always be called inside a loop that checks the
   condition it is sleeping on.
   For example:

       while ( ( b->flags & ( B_VALID | B_DIRTY ) ) != B_VALID )
       {
            sleep( b, &idelock );
       }

    This handles two scenarios:
    . If multiple processes are sleeping on a channel, a call to wakeup
      will wake them all
    . The first to run does its thing and may leave the sleep condition as
      no longer true (for ex might read all the new data)
    . In this case, despite being woken up, the condition is no longer
      true for all the other processes
    . From their perspective, the wakeup was "spurious"
    . If inside a condition loop, they will sleep again

    . The second scenario arises if two unrelated 'chan' are accidentally
      the same value
    . For the process that the wakeup was not intended for, it
      will view the wakeup as spurious since the sleep condition is not met
*/
void sleep ( void* chan, struct spinlock* lk )
{
	struct proc* p;

	p = myproc();

	if ( p == 0 )
	{
		panic( "sleep" );
	}

	if ( lk == 0 )
	{
		panic( "sleep: without lk" );
	}

	/* Must acquire ptable.lock in order to change p->state
	   and then call sched.

	   Once we hold ptable.lock, we can be guaranteed that we won't
	   miss any wakeup since wakeup runs with ptable.lock locked.
	   It is now safe to release lk... p.69
	*/
	if ( lk != &ptable.lock )
	{
		acquire( &ptable.lock );

		release( lk );
	}

	// Go to sleep.
	p->chan  = chan;
	p->state = SLEEPING;

	sched();


	// - - - - - - - - - - - - - - - - -
	// Continues from here after wakeup


	// Tidy up.
	p->chan = 0;

	// Reacquire original lock.
	if ( lk != &ptable.lock )
	{
		release( &ptable.lock );

		acquire( lk );
	}
}

// Wake up all processes sleeping on chan.
// The ptable lock must be held.
/* Causes the processes's sleep calls to return.
*/
static void wakeup1 ( void* chan )
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

// Wake up all processes sleeping on chan ("thundering herd")
void wakeup ( void* chan )
{
	acquire( &ptable.lock );

	wakeup1( chan );

	release( &ptable.lock );
}


// _________________________________________________________________________________

// Print a process listing to console. For debugging.
// Runs when user types Ctrl+P on console.
// No lock to avoid wedging a stuck machine further.
#define PROCNPCS 10

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
	uint         pcs [ PROCNPCS ];

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
			getcallerpcs( ( uint* ) p->context->ebp + 2, pcs, PROCNPCS );

			cprintf( "call stack:\n" );

			for ( i = 0; i < PROCNPCS; i += 1 )
			{
				if ( pcs[ i ] == 0 )
				{
					break;
				}

				cprintf( "    %p\n", pcs[ i ] );
			}

			cprintf( "\n" );
		}
	}

	cprintf( "\n" );
}
