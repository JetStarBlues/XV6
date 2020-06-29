/* Based on:
   . https://github.com/DoctorWkt/xv6-freebsd/blob/master/include/xv6/termios.h
   . https://github.com/DoctorWkt/xv6-freebsd/blob/master/lib/xv6/tcgetattr.c
   . https://github.com/nyuichi/xv6/commit/790603230625332d23459ff9ef493c6fb81ea92c

   See this great article for more on terminal control:
   . https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
   And associated man page:
   . http://man7.org/linux/man-pages/man3/termios.3.html
*/

/* Only interested in ability to switch between canonical and
   raw mode input. As such, my implementation deviates from the
   standard (see man termio.h) in favor of simplicity.
*/


// __________________________________________________________________________

#include "kernel/types.h"
#include "kernel/termios.h"
#include "user.h"

/* Used to configure the console:
     . switch between canonical-mode and raw-mode input
     . turn input-echoing on or off
*/


/* Mirrors behaviour of 'tcgetattr'.
   Returns 0 on success, -1 on error.
*/
int getConsoleAttr ( int fd, struct termios* termios_p )
{
	return ioctl( fd, CONS_IOCTL_GETATTR, termios_p );
}

/* Mirrors behaviour of 'tcsetattr'.
   Returns 0 on success, -1 on error.
*/
int setConsoleAttr ( int fd, struct termios* termios_p )
{
	return ioctl( fd, CONS_IOCTL_SETATTR, termios_p );
}
