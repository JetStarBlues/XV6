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

//
#define CONS_IOCTL_GETATTR 1
#define CONS_IOCTL_SETATTR 2

//
struct termios {

	/* If set, enables echoing of input characters.
	*/
	uchar echo;

	/* If set, uses canonical mode input.
	   Otherwise uses raw mode.
	*/
	uchar icanon;

	/* If set, Ctrl+C (SIGINT), Ctrl+Z (SIGTSTOP)
	   generate correspoinding signals.
	   Currently does nothing, as signals not implemented.
	*/
	uchar isig;
};

//
int getConsoleAttr ( int fd, struct termios* termios_p );
int setConsoleAttr ( int fd, struct termios* termios_p );
