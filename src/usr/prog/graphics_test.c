#include "types.h"
#include "user.h"
#include "fcntl.h"
// #include "vga.h"
#include "display.h"

/*
Based on:
	. https://github.com/sam46/xv6/blob/master/imshow.c
*/

#define GFXMODE 0x13
#define WIDTHxHEIGHT_GFXMODE 64000

int main ( int argc, char* argv [] )
{
	int displayfd;
	int imagefd;

	uint argp [ 3 ];

	// int  i;
	int  bytesRead;
	char imgbuf [ WIDTHxHEIGHT_GFXMODE ];


	if ( ( displayfd = open( "/dev/display", O_RDWR ) < 0 ) )
	{
		printf( 2, "graphics_test: cannot open display\n" );

		exit();
	}

	if ( ( imagefd = open( "/usr/pics/cam2_dribbble-drawsgood.bin", O_RDONLY ) < 0 ) )
	{
		printf( 2, "graphics_test: cannot open image\n" );

		exit();
	}


	// Retrieve contents of image file
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


	// Switch to graphics mode
	argp[ 0 ] = GFXMODE;
	ioctl( displayfd, DISP_SETMODE, argp );


	// Blit the image
	argp[ 0 ] = ( uint ) imgbuf;
	ioctl( displayfd, DISP_BLIT, argp );





	exit();
}
