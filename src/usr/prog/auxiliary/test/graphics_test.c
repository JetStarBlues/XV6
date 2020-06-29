#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "GFX.h"

/*
Based on:
	. https://github.com/sam46/xv6/blob/master/imshow.c
*/

#define UINPUTBUFSZ 3


int main ( int argc, char* argv [] )
{
	int   imagefd;
	int   bytesRead;
	char* imgbuf;
	char  uinputbuf [ UINPUTBUFSZ ];


	// imagefd = open( "/usr/pics/paltest.bin", O_RDONLY );
	imagefd = open( "/usr/pics/cam2.bin", O_RDONLY );

	if ( imagefd < 0 )
	{
		printf( 2, "graphics_test: cannot open image\n" );

		exit();
	}


	// Retrieve contents of image file
	printf( 1, "Reading image from disk...\n" );

	imgbuf = ( char* ) malloc( SCREEN_WIDTHxHEIGHT );

	if ( imgbuf == NULL )
	{
		printf( 2, "graphics_test: failed to malloc\n" );

		exit();
	}

	bytesRead = read( imagefd, imgbuf, SCREEN_WIDTHxHEIGHT );

	if ( bytesRead != SCREEN_WIDTHxHEIGHT )
	{
		printf( 2,

			"graphics_test: expected to read %d bytes, but read %d\n",
			SCREEN_WIDTHxHEIGHT,
			bytesRead
		);

		exit();
	}


	// Switch to graphics mode
	GFX_init();


	// Blit the image
	printf( 1, "Drawing image...\n" );

	GFX_blit( ( uchar* ) imgbuf );


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
	GFX_exit();


	//
	free( imgbuf );  // deallocate memory, so it can be used for other things (no garbage collection)

	//
	printf( 1, "Hasta luego!\n" );

	exit();
}
