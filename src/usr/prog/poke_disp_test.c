#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "display.h"


#define TXTMODE 0x03
#define GFXMODE 0x13

#define UMMIO__VGA_MODE13_BUF 0x7FFF0000  // Temp hardcode

#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 200

#define UINPUTBUFSZ 3


int main ( int argc, char* argv [] )
{
	int   displayfd;
	char  uinputbuf [ UINPUTBUFSZ ];
	char* vgaBuffer;
	int   x, y, c;


	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( 2, "graphics_test: cannot open display\n" );

		exit();
	}


	//
	printf( 1, "Switching to graphics mode...\n" );

	// Switch to graphics mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, GFXMODE );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL );


	//
	printf( 1, "Clearing screen...\n" );

	ioctl(

		displayfd,
		DISP_IOCTL_DRAWFILLRECT,
		0,                        // x
		0,                        // y
		SCREEN_WIDTH,             // w
		SCREEN_HEIGHT,            // h
		67                        // color
	);

	//
	printf( 1, "Poking colors...\n" );

	vgaBuffer = ( char* ) UMMIO__VGA_MODE13_BUF;

	for ( y = 0; y < 10; y += 1 )
	{
		c = 0;

		for ( x = 0; x < 15; x += 1 )
		{
			*( vgaBuffer + ( y * SCREEN_WIDTH ) + x ) = c;

			printf( 1, "c %d at (%d, %d)\n", c, x, y );

			c += 1;
		}
	}
	// Should probably clear screen first...


	/* When user enters "q", switch back to text mode.
	   Ideally this would be an "onkeypress" event handler;
	   for now we sleep on stdin
	*/
	while ( 1 )
	{
		memset( uinputbuf, 0, UINPUTBUFSZ );

		gets( uinputbuf, UINPUTBUFSZ );

		uinputbuf[ strlen( uinputbuf ) - 1 ] = 0;  // remove newline char '\n'

		if ( strcmp( uinputbuf, "q" ) == 0 )
		{
			break;
		}
	}


	// Switch to text mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, TXTMODE );

	printf( 1, "Hasta luego!\n" );


	//
	exit();
}
