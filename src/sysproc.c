#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
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

// Return number of clock tick interrupts since start
int sys_uptime ( void )
{
	uint xticks;

	acquire( &tickslock );

	xticks = ticks;

	release( &tickslock );

	return xticks;
}

/* Retrieve current date and time (GMT timezone)
   from the CMOS RTC
    http://panda.moyix.net/~moyix/cs3224/fall16/hw4/hw4.html
    https://github.com/moyix/xv6-public/blob/hw4/sysproc.c
*/
int sys_gettime ( void )
{
	struct rtcdate* d;

	if ( argptr( 0, ( char** ) &d, sizeof( struct rtcdate ) ) < 0 )
	{
		return - 1;
	}

	cmostime( d );

	return 0;
}
