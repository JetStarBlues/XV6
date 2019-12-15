#include "types.h"
#include "defs.h"
#include "date.h"


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

	if ( argptr( 0, ( void* ) &d, sizeof( struct rtcdate ) ) < 0 )
	{
		return - 1;
	}

	cmostime( d );

	return 0;
}
