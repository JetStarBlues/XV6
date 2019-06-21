// Mutual exclusion spin locks.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
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
// Loops ( spins ) until the lock is acquired.
// Holding a lock for a long time may cause
// other CPUs to waste time spinning to acquire it.
void acquire ( struct spinlock* lk )
{
	pushcli();  // disable interrupts to avoid deadlock.

	if ( holding( lk ) )
	{
		panic( "acquire" );
	}

	// The xchg is atomic.
	while ( xchg( &lk->locked, 1 ) != 0 )
	{
		// spin, waiting for lock to become available
	}

	// Tell the C compiler and the processor to not move loads or stores
	// past this point, to ensure that the critical section's memory
	// references happen after the lock is acquired.
	__sync_synchronize();

	// Record info about lock acquisition for debugging.
	lk->cpu = mycpu();

	getcallerpcs( &lk, lk->pcs );
}

// Release the lock.
void release ( struct spinlock* lk )
{
	if ( ! holding( lk ) )
	{
		panic( "release" );
	}

	lk->pcs[ 0 ] = 0;
	lk->cpu      = 0;

	// Tell the C compiler and the processor to not move loads or stores
	// past this point, to ensure that all the stores in the critical
	// section are visible to other cores before the lock is released.
	// Both the C compiler and the hardware may re-order loads and
	// stores; __sync_synchronize() tells them both not to.
	__sync_synchronize();

	// Release the lock, equivalent to lk->locked = 0.
	// This code can't use a C assignment, since it might
	// not be atomic. A real OS would use C atomics here.
	asm volatile( "movl $0, %0" : "+m" ( lk->locked ) :  );

	popcli();
}

// Check whether this cpu is holding the lock.
int holding ( struct spinlock* lock )
{
	int r;

	pushcli();

	r = lock->locked && lock->cpu == mycpu();

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
		panic( "popcli - interruptible" );
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


// __________________________________________________________________________________

// Record the current call stack in pcs[] by following the %ebp chain.
void getcallerpcs ( void* v, uint pcs [] )
{
	uint* ebp;
	int   i;

	ebp = ( uint* ) v - 2;

	for ( i = 0; i < 10; i += 1 )
	{
		if ( ebp == 0 ||
			 ebp <  ( uint* ) KERNBASE ||
			 ebp == ( uint* ) 0xffffffff )
		{
			break;
		}

		pcs[ i ] = ebp[ 1 ];       // saved %eip

		ebp = ( uint* ) ebp[ 0 ];  // saved %ebp
	}

	for ( ; i < 10; i += 1 )
	{
		pcs[ i ] = 0;
	}
}
