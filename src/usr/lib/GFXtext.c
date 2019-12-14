#include "types.h"
#include "user.h"
#include "display.h"
#include "fonts.h"
#include "fcntl.h"
#include "GFXtext.h"

/* Simple interface...

   Currently exists to facilitate kEditor.
   Refine API if intend to use elsewhere.

   Caller is responsible for:
   . keeping track of cursor position
   . calling 'initGFXText' before use, and 'exitGFXText' after use.
     Or alternatively, supplying a displayfd via 'setDisplayFd'

*/

// VGA modes
#define TXTMODE 0x03
#define GFXMODE 0x13

// Assuming VGA Graphics mode (0x13)
#define SCREEN_WIDTH  320
#define SCREEN_HEIGHT 200

// Assuming using font g_8x11_font__Gohu
#define FONT_WIDTH  8
#define FONT_HEIGHT 11

// Dimensions in characters
static uint nRows = 18;  // 200 / 11
static uint nCols = 40;  // 320 / 8

// Cursor position
static uint cRow;
static uint cCol;

// Default colors (standard 256-color VGA palette)
static uchar textColor   = 24;   // grey
static uchar textBgColor = 15;   // white
static uchar cursorColor = 112;  // brown

//
static int displayfd = 0;



// ____________________________________________________________________________________

void initGFXText ( void )
{
	uint argp [ 1 ];

	//
	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( 2, "graphics_test: cannot open display\n" );

		exit();
	}


	printf( 1, "Switching to graphics mode...\n" );

	// Switch to graphics mode
	argp[ 0 ] = GFXMODE;
	ioctl( displayfd, DISP_IOCTL_SETMODE, argp );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL, 0 );


	// Garbage otherwise...
	clearScreen();
}

void exitGFXText ( void )
{
	uint argp [ 1 ];

	// Switch to text mode
	argp[ 0 ] = TXTMODE;
	ioctl( displayfd, DISP_IOCTL_SETMODE, argp );

	printf( 1, "Switched to text mode\n" );


	//
	close( displayfd );
}

void setDisplayFd ( int fd )
{
	displayfd = fd;
}


// ____________________________________________________________________________________

void setCursorPosition ( uint row, uint col )
{
	if ( ( row >= 0 ) && ( row < nRows ) )
	{
		cRow = row;
	}

	if ( ( col >= 0 ) && ( col < nCols ) )
	{
		cCol = col;
	}
}

void getCursorPosition ( uint* rowPtr, uint* colPtr )
{
	*rowPtr = cRow;
	*colPtr = cCol;
}


// ____________________________________________________________________________________

void setTextColor ( uchar color )
{
	textColor = color;
}
void setTextBgColor ( uchar color )
{
	textBgColor = color;
}
void setCursorColor ( uchar color )
{
	cursorColor = color;
}
void invertTextColors ( void )
{
	uchar tmp;

	tmp         = textColor;
	textColor   = textBgColor;
	textBgColor = tmp;
}


// ____________________________________________________________________________________

void clearScreen ( void )
{
	uint argp [ 5 ];

	argp [ 0 ] = 0;                     // x
	argp [ 1 ] = 0;                     // y
	argp [ 2 ] = SCREEN_WIDTH;          // w
	argp [ 3 ] = SCREEN_HEIGHT;         // h
	argp [ 4 ] = ( uint ) textBgColor;  // color

	ioctl( displayfd, DISP_IOCTL_DRAWFILLRECT, argp );
}


// ____________________________________________________________________________________

// Simple, terrible perfomance
// Assumes using font g_8x11_font__Gohu
static void drawFontChar ( uchar ch, uint x, uint y )
{
	uchar chRow;
	uchar pixelMask;
	int   i;
	uint  j;
	uint  baseAddress;
	uint  argp [ 3 ];

	//
	argp[ 0 ] = x;
	argp[ 1 ] = y;
	argp[ 2 ] = ( uint ) textColor;

	// Base location of character's pixel info
	baseAddress = ch * FONT_HEIGHT;


	// For each row in the character...
	for ( j = 0; j < FONT_HEIGHT; j += 1 )
	{
		// Get row
		chRow = ( uchar ) g_8x11_font__Gohu[ baseAddress + j ];

		// Draw each pixel in the current row
		for ( i = FONT_WIDTH - 1; i >= 0; i -= 1 )
		{
			pixelMask = ( uint ) ( 1 << i );

			if ( ( chRow & pixelMask ) != 0 )
			{
				ioctl( displayfd, DISP_IOCTL_DRAWPIXEL, argp );
			}

			// Update x
			argp[ 0 ] += 1;
		}

		// Reset x
		argp[ 0 ] = x;

		// Update y
		argp[ 1 ] += 1;
	}
}

void printChar ( uchar ch )
{
	uint x;
	uint y;
	uint argp [ 5 ];

	//
	x = cCol * FONT_WIDTH;
	y = cRow * FONT_HEIGHT;

	// printf( 1, "%d -> %d, %d\n", ( uint ) ch, x, y );

	// Draw rect for char bg
	argp [ 0 ] = x;                     // x
	argp [ 1 ] = y;                     // y
	argp [ 2 ] = FONT_WIDTH;            // w
	argp [ 3 ] = FONT_HEIGHT;           // h
	argp [ 4 ] = ( uint ) textBgColor;  // color

	ioctl( displayfd, DISP_IOCTL_DRAWFILLRECT, argp );


	// Draw char
	drawFontChar( ch, x, y );
}


// ____________________________________________________________________________________

void drawCursor ( void )
{
	uint x;
	uint y;
	uint argp [ 5 ];

	//
	x = cCol * FONT_WIDTH;
	y = cRow * FONT_HEIGHT;

	// Draw rect for char bg
	argp [ 0 ] = x;                     // x
	argp [ 1 ] = y;                     // y
	argp [ 2 ] = 1;                     // w
	argp [ 3 ] = FONT_HEIGHT;           // h
	argp [ 4 ] = ( uint ) cursorColor;  // color

	ioctl( displayfd, DISP_IOCTL_DRAWFILLRECT, argp );
}
