// Sleeping locks

/* Sometimes a lock needs to held for a long time, and thus
   spinning is un-ideal.

   Sleeplocks support yielding the CPU during their critcal sections ??

   Because sleeplocks leave interrupts enabled, they cannot be used:
     . in interrupt handlers ??
     . inside a spinlock's critical section ??
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "x86.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"
#include "sleeplock.h"

void initsleeplock ( struct sleeplock* slk, char* name )
{
	initlock( &slk->lock, "sleep lock" );

	slk->locked = 0;

	// Init debugging info
	slk->name = name;
	slk->pid  = 0;
}

/* Yields the CPU while waiting to acquire the lock? and does not
   disable interrupts?

   Revisit explanation on p.58
*/
void acquiresleep ( struct sleeplock* slk )
{
	// Acquire spinlock...
	acquire( &slk->lock );

	// While sleeplock is held by someone else, sleep...
	while ( slk->locked )
	{
		sleep( slk, &slk->lock );
	}

	// Acquire sleeplock...
	slk->locked = 1;

	// Set debugging info
	slk->pid = myproc()->pid;

	// Release spinlock...
	release( &slk->lock );
}

void releasesleep ( struct sleeplock* slk )
{
	// Acquire spinlock...
	acquire( &slk->lock );

	// Clear debugging info
	slk->pid = 0;

	// Release sleeplock...
	slk->locked = 0;

	// Wakeup processes sleeping on the sleeplock...
	wakeup( slk );

	// Releease spinlock...
	release( &slk->lock );
}

// Does the current process hold the sleeplock
int holdingsleep ( struct sleeplock* slk )
{
	int r;
	
	acquire( &slk->lock );

	r = slk->locked && ( slk->pid == myproc()->pid );

	release( &slk->lock );

	return r;
}
