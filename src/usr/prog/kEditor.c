/*
  Build your own text editor
  Jeremy Ruten
  https://viewsourcecode.org/snaptoken/kilo/index.html
*/

/*

TODO:
	. 

*/

#include "types.h"
#include "fcntl.h"
#include "user.h"

void enableRawMode ( void )
{
	/* Using 'termios.h' protocol, would do this by
       clearing ICANON flag...

         . https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html
         . https://github.com/SerenityOS/serenity/blob/04b9dc2d30cfc9b383029f6a4b02e2725108b0ae/Kernel/TTY/TTY.h
         . https://github.com/SerenityOS/serenity/blob/04b9dc2d30cfc9b383029f6a4b02e2725108b0ae/Libraries/LibC/termios.h
	*/

	/*
	   . clear ECHO flag
	   . clear ICANON flag
	   . clear ...
	*/
}

int main ( void )
{
	char c;

	while ( read( 1, &c, 1 ) == 1 )
	{
		if ( c == 'q' )
		{
			break;
		}

		printf( 1, "(%c)\n", c );
	}

	exit();
}
