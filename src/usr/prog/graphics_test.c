#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "display.h"

/*
Based on:
	. https://github.com/sam46/xv6/blob/master/imshow.c
*/

#define TXTMODE              0x03
#define GFXMODE              0x13
#define WIDTHxHEIGHT_GFXMODE 64000

#define UINPUTBUFSZ 3


int main ( int argc, char* argv [] )
{
	int   displayfd;
	int   imagefd;
	int   bytesRead;
	char* imgbuf;
	char  uinputbuf [ UINPUTBUFSZ ];


	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( 2, "graphics_test: cannot open display\n" );

		exit();
	}

	// imagefd = open( "/usr/pics/paltest.bin", O_RDONLY );
	imagefd = open( "/usr/pics/cam2.bin", O_RDONLY );

	if ( imagefd < 0 )
	{
		printf( 2, "graphics_test: cannot open image\n" );

		exit();
	}


	// Retrieve contents of image file
	printf( 1, "Reading image from disk...\n" );

	imgbuf = ( char* ) malloc( WIDTHxHEIGHT_GFXMODE );

	if ( imgbuf == NULL )
	{
		printf( 2, "graphics_test: failed to malloc\n" );

		exit();
	}

	bytesRead = read( imagefd, imgbuf, WIDTHxHEIGHT_GFXMODE );

	if ( bytesRead != WIDTHxHEIGHT_GFXMODE )
	{
		printf( 2,

			"graphics_test: expected to read %d bytes, but read %d\n",
			WIDTHxHEIGHT_GFXMODE,
			bytesRead
		);

		exit();
	}


	printf( 1, "Drawing image...\n" );

	// Switch to graphics mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, GFXMODE );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL );

	// Blit the image
	ioctl( displayfd, DISP_IOCTL_BLIT, imgbuf );


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
	free( imgbuf );  // deallocate memory, so it can be used for other things (no garbage collection)

	exit();
}
