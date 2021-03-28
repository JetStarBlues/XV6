#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/display.h"
#include "user.h"
#include "GFX.h"

/* Simple interface...

   Caller is responsible for:
   . calling 'GFX_init' before use, and 'GFX_exit' after use

*/

// ____________________________________________________________________________________

// Screen framebuffer
static uchar* gfxbuffer = ( uchar* ) GFXBUFFER;

//
static int displayfd = 0;


// ____________________________________________________________________________________

void GFX_init ( void )
{
	//
	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( stderr, "GFX_init: cannot open display\n" );

		exit();
	}


	printf( stdout, "Switching to graphics mode...\n" );

	// Switch to graphics mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, VGA_GFXMODE );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL );


	// Garbage otherwise...
	// GFX_clearScreen( 15 );  // white
}

void GFX_exit ( void )
{
	// Switch to text mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, VGA_TXTMODE );

	printf( stdout, "Switched to text mode\n" );


	//
	close( displayfd );
}


// ____________________________________________________________________________________

void GFX_clearScreen ( uchar clearColor )
{
	memset( gfxbuffer, clearColor, SCREEN_WIDTHxHEIGHT );
}


// ____________________________________________________________________________________

void GFX_drawPixel ( int x, int y, uchar colorIdx )
{
	int off;

	// Should check bounds. Ignoring for speed.

	off = SCREEN_WIDTH * y + x;

	gfxbuffer[ off ] = colorIdx;
}

void GFX_blit ( uchar* src )
{
	memcpy( gfxbuffer, src, SCREEN_WIDTHxHEIGHT );
}


// ____________________________________________________________________________________

void GFX_fillRect ( int x, int y, int w, int h, uchar colorIdx )
{
	int off;
	int x2;
	int y2;

	// Should check bounds. Ignoring for speed.

	// If screen dimensions, memset
	if ( ( x == 0 ) && ( y == 0 ) && ( w == SCREEN_WIDTH ) && ( h == SCREEN_HEIGHT ) )
	{
		memset( gfxbuffer, colorIdx, SCREEN_WIDTHxHEIGHT );
	}

	// Otherwise do math...
	else
	{
		for ( y2 = y; y2 < ( y + h ); y2 += 1 )
		{
			off = SCREEN_WIDTH * y2 + x;

			for ( x2 = x; x2 < ( x + w ); x2 += 1 )
			{
				gfxbuffer[ off ] = colorIdx;

				off += 1;
			}
		}
	}
}


// ____________________________________________________________________________________

/* Draws a bitmap of width 8, height 'h'.
   Encoding:
    . Each byte represents a row.
    . A set bit represents a pixel.
*/
/*void GFX_drawBitmap8 ( uchar* bitmap, int x, int y, int h, uchar colorIdx )
{
	uchar bitmapRow;
	uchar pixelMask;
	int   i;
	int   j;
	int   y2;
	int   off;

	// Should check bounds. Ignoring for speed.

	y2 = y;

	// For each row (byte) in the bitmap...
	for ( j = 0; j < h; j += 1 )
	{
		bitmapRow = bitmap[ j ];

		off = SCREEN_WIDTH * y2 + x;

		// For each bit in the row...
		for ( i = 7; i >= 0; i -= 1 )
		{
			pixelMask = 1 << i;

			// If the bit is set, draw a pixel
			if ( ( bitmapRow & pixelMask ) != 0 )
			{
				gfxbuffer[ off ] = colorIdx;
			}

			off += 1;
		}

		y2 += 1;
	}
}*/
