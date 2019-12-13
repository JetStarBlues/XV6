#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"


int sys_fork ( void )
{
	return fork();
}

int sys_exit ( void )
{
	exit();

	return 0;  // not reached
}

int sys_wait ( void )
{
	return wait();
}

int sys_kill ( void )
{
	int pid;

	if ( argint( 0, &pid ) < 0 )
	{
		return - 1;
	}

	return kill( pid );
}

int sys_getpid ( void )
{
	return myproc()->pid;
}

/* getppid could be implemented as myproc()->parent->pid
*/

int sys_sbrk ( void )
{
	int addr;
	int n;

	if ( argint( 0, &n ) < 0 )  // n is not negative...
	{
		return - 1;
	}

	addr = myproc()->sz;

	if ( growproc( n ) < 0 )
	{
		return - 1;
	}

	return addr;
}

int sys_sleep ( void )
{
	int  n;
	uint ticks0;

	if ( argint( 0, &n ) < 0 )
	{
		return - 1;
	}

	acquire( &tickslock );  // lock is held because ??

	ticks0 = ticks;

	while ( ticks - ticks0 < n )
	{
		if ( myproc()->killed )
		{
			release( &tickslock );

			return - 1;
		}

		sleep( &ticks, &tickslock );
	}

	release( &tickslock );

	return 0;
}


/* Is 'select' based on sleep/wakeup 
*/
