/* Based on:
   . https://github.com/DoctorWkt/xv6-freebsd/blob/master/include/xv6/termios.h
   . https://github.com/nyuichi/xv6

   See this great article for more on terminal control:
   . https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
   And associated man page:
   . http://man7.org/linux/man-pages/man3/termios.3.html
*/

/* Only interested in ability to switch between canonical and
   raw mode input. As such, my implementation deviates from the
   standard (see man termio.h) in favor of simplicity.
*/

struct termios {

	/* If set, enables echoing of input characters.
	*/
	.

	/* If set, uses canonical mode input.
	   Otherwise uses raw mode.
	*/
	.

	/* If set, Ctrl+C (SIGINT), Ctrl+Z (SIGTSTOP)
	   generate correspoinding signals.
	   Currently does nothing, as signals not implemented.
	*/
	.
};


/* If syscall, instead of ioctl, don't need fd...
   Otherwise need fd corresponding with device, console is 0/1/2
*/

/* Returns 0 on success, -1 on error...
*/

int getConsoleAttr
int tcgetattr ( int fd, struct termios* termios_p )
{
	.
}

int setConsoleAttr
int tcsetattr ( int fd, int optional_actions, struct termios* termios_p )
{
	.
}


// Initial values...
tp->echo   = 1;
tp->icanon = 1;
tp->isig   = 0;

