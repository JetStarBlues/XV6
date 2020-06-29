#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/display.h"
#include "user.h"
#include "GFX.h"


#define UINPUTBUFSZ 3


int main ( int argc, char* argv [] )
{
	int   displayfd;
	char  uinputbuf [ UINPUTBUFSZ ];
	char* vgaBuffer;
	int   x, y, w, k;
	int   color;


	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( 2, "poke_disp_test: cannot open display\n" );

		exit();
	}


	//
	printf( 1, "Switching to graphics mode...\n" );

	// Switch to graphics mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, VGA_GFXMODE );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL );


	//
	printf( 1, "Clearing screen...\n" );

	vgaBuffer = ( char* ) GFXBUFFER;

	memset( vgaBuffer, 67, SCREEN_WIDTHxHEIGHT );


	//
	printf( 1, "Poking colors...\n" );

	// Draw colored squares
	w = 10;
	for ( y = 0; y < w; y += 1 )
	{
		color = 0;

		for ( k = 0; k < 16; k += 1 )
		{
			for ( x = k * w; x < k * w + w; x += 1 )
			{
				*( vgaBuffer + ( y * SCREEN_WIDTH ) + x ) = color;  // poke

				// printf( 1, "c %d at (%d, %d)\n", color, x, y );
			}

			color += 1;
		}
	}


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
	ioctl( displayfd, DISP_IOCTL_SETMODE, VGA_TXTMODE );

	printf( 1, "Hasta luego!\n" );


	//
	exit();
}
