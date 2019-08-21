// Mutual exclusion spin locks

/* Any code that accesses shared data must have a strategy for
   maintaining correctness despite concurrency.

   Concurrency may arise from:
     . multiple threads
     . multiple cores (CPUs)
     . interrupt handler?

   A lock is one such strategy.

   Interrupts deserve special attention
     . If an interrupt handler uses a lock, and the process it
       interrupted holds the lock, a deadlock is created.

     . The process needs to continue what it was doing before the
       interrupt to eventually release the lock
     . The interrupt handler however, will not progress (and eventually
       RETI) until it acquires the lock.

      . To avoid this, the lock in question should never be held while
        interrupts are enabled.

      . xv6 is conservative and ensures interrupts are disabled when
        *any* lock is held
      . As such, xv6 must do some book-keeping to track the nesting
        level of locks:
        . acquire calls pushcli, which increments the count cpu->ncli
        . release calls popcli, which decrements the count
        . When the count reaches zero, popcli restores the previous
          interrupt enable state

      . Disabling interrupts while a lock is held also means that
        pre-emptive context switches via timer interrupts can't happen...
      . This is where sleeplocks come into play...


   It is important for efficiency to not lock too much, as locks
   reduce parallelism...

   If must hold several locks at the same time, it is important that
   they are always acquired in the same order to avoid deadlock.
   For ex, xv6 acquires file system related locks in the following
   order: ??
     . lock on directory (?)
     . lock on the file inode (?)
     . lock on disk block buffer (bcache->lock?)
     . lock on the disk request queue (idelock)
     . lock on the process table (ptable->lock)


   List of locks currently used in xv6:

     . ptable->lock
       . ??
     . kmem->lock
       . held when modifying freelist
     . buf->lock
       . ??
       . is a sleeplock
     . ftable->lock
       . ??
     . inode->lock
       . ??
       . is a sleeplock
         . for example, held while reading and writing a file's content
           to disk, which can take a long time...
     . icache->lock
       . ??
     . bcache->lock
       . ??
     . log->lock
       . ??
     . idelock
       . held when modifying idequeue
     . tickslock
       . held when modifying ticks
     . cons-lock
       . ??
     . pipe->lock
       . ??
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"

void initlock ( struct spinlock* lk, char* name )
{
	lk->name   = name;
	lk->locked = 0;
	lk->cpu    = 0;
}

// Acquire the lock.
// Loops (spins) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void acquire ( struct spinlock* lk )
{
	pushcli();  // disable interrupts to avoid deadlock.

	if ( holding( lk ) )
	{
		panic( "acquire" );
	}


	// The xchg is atomic
	/* Atomically 1) retrieve the current value of lk->locked and
	   2) set its value to 1 (held)

	   If the lock is currently held, the retrieved value will be 1,
	   otherwise the retrieved value will be 0.

	   If the retrieved value is 0, it means we have successfully
	   acquired the lock. We loop until we acquire the lock.
	*/
	while ( xchg( &lk->locked, 1 ) != 0 )
	{
		// spin, waiting for lock to become available
	}


	/* Both the C compiler and the CPU may re-order loads and
	   stores (to acheive higher perfomace); __sync_synchronize() tells
	   them not to.

	   Here, we want to ensure that the critical section's memory
	   references happen after the lock is acquired...
	*/
	__sync_synchronize();


	// Record info about lock acquisition for debugging.
	lk->cpu = mycpu();  // CPU that holds the lock

	getcallerpcs( &lk, lk->pcs, SPLKNPCS );  // call stack that acquired the lock
}

// Release the lock.
void release ( struct spinlock* lk )
{
	if ( ! holding( lk ) )
	{
		panic( "release" );
	}

	// Clear associated debugging info
	lk->pcs[ 0 ] = 0;
	lk->cpu      = 0;


	/* Both the C compiler and the CPU may re-order loads and
	   stores (to acheive higher perfomace); __sync_synchronize() tells
	   them not to.

	   Here, we want to ensure that all the stores in the critical
	   section are visible to other CPUs before the lock is released...
	*/
	__sync_synchronize();


	// Release the lock, equivalent to lk->locked = 0.
	/* This code can't use a C assignment, since it might not be atomic.
	   A real OS would use C atomics here to make the code portable
	   to different CPU architectures
	*/
	asm volatile( "movl $0, %0" : "+m" ( lk->locked ) :  );


	popcli();  // enable interrupts...
}

// Check whether this cpu is holding the lock.
int holding ( struct spinlock* lock )
{
	int r;

	pushcli();

	r = lock->locked && ( lock->cpu == mycpu() );

	popcli();

	return r;
}


// __________________________________________________________________________________

// Pushcli/popcli are like cli/sti except that they are matched:
// it takes two popcli to undo two pushcli. Also, if interrupts
// are off, then pushcli, popcli leaves them off.

void pushcli ( void )
{
	int eflags;

	eflags = readeflags();

	// Disable interrupts
	cli();

	// Save interrupt state at start of outermost
	if ( mycpu()->ncli == 0 )
	{
		mycpu()->intena = eflags & FL_IF;
	}

	// Track depth of pushcli nesting
	mycpu()->ncli += 1;
}

void popcli ( void )
{
	// If interrupts are enabled, panic...
	if ( readeflags() & FL_IF )
	{
		panic( "popcli: interruptible" );
	}

	// Track depth of pushcli nesting
	mycpu()->ncli -= 1;

	// Popped more than were pushed...
	if ( mycpu()->ncli < 0 )
	{
		panic( "popcli" );
	}

	// Reached outermost, so restore interrupt state
	if ( mycpu()->ncli == 0 && mycpu()->intena )
	{
		sti();  // enable interrupts
	}
}
