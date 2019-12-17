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

// Font
#define FONT        g_8x11_font__Gohu
#define FONT_WIDTH  8
#define FONT_HEIGHT 11

// Dimensions in characters
static uint nRows = 18;  // 200 / 11
static uint nCols = 40;  // 320 / 8

// Cursor position
static uint cursorRow;
static uint cursorCol;

// Default colors (standard 256-color VGA palette)
static uchar textColor   = 24;   // grey
static uchar textBgColor = 15;   // white
static uchar cursorColor = 112;  // brown

//
static int displayfd = 0;



// ____________________________________________________________________________________

void initGFXText ( void )
{
	//
	displayfd = open( "/dev/display", O_RDWR );

	if ( displayfd < 0 )
	{
		printf( 2, "graphics_test: cannot open display\n" );

		exit();
	}


	printf( 1, "Switching to graphics mode...\n" );

	// Switch to graphics mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, GFXMODE );

	// Set palette
	ioctl( displayfd, DISP_IOCTL_DEFAULTPAL );


	// Garbage otherwise...
	clearScreen();
}

void exitGFXText ( void )
{
	// Switch to text mode
	ioctl( displayfd, DISP_IOCTL_SETMODE, TXTMODE );

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
		cursorRow = row;
	}

	if ( ( col >= 0 ) && ( col < nCols ) )
	{
		cursorCol = col;
	}
}

void getCursorPosition ( uint* rowPtr, uint* colPtr )
{
	*rowPtr = cursorRow;
	*colPtr = cursorCol;
}

void getDimensions ( uint* nRowsPtr, uint* nColsPtr )
{
	*nRowsPtr = nRows;
	*nColsPtr = nCols;
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
	ioctl(

		displayfd,
		DISP_IOCTL_DRAWFILLRECT,
		0,                        // x
		0,                        // y
		SCREEN_WIDTH,             // w
		SCREEN_HEIGHT,            // h
		textBgColor               // color
	);
}


// ____________________________________________________________________________________

/*
// Simple, terrible performance.
static void drawFontChar ( uchar ch, uint x, uint y )
{
	uchar chRow;
	uchar pixelMask;
	int   i;
	uint  j;
	uint  baseAddress;
	uint  x2;
	uint  y2;

	//
	x2 = x;
	y2 = y;

	// Base location of character's pixel info
	baseAddress = ch * FONT_HEIGHT;


	// For each row in the character...
	for ( j = 0; j < FONT_HEIGHT; j += 1 )
	{
		// Get row
		chRow = ( uchar ) FONT[ baseAddress + j ];

		// Draw each pixel in the current row
		for ( i = FONT_WIDTH - 1; i >= 0; i -= 1 )
		{
			pixelMask = ( uint ) ( 1 << i );

			if ( ( chRow & pixelMask ) != 0 )
			{
				ioctl(

					displayfd,
					DISP_IOCTL_DRAWPIXEL,
					x2,                    // x
					y2,                    // y
					textColor              // color
				);
			}

			// Update x
			x2 += 1;
		}

		// Reset x
		x2 = x;

		// Update y
		y2 += 1;
	}
}*/

void printChar ( uchar ch )
{
	uint x;
	uint y;

	//
	x = cursorCol * FONT_WIDTH;
	y = cursorRow * FONT_HEIGHT;

	// printf( 1, "%d -> %d, %d\n", ( uint ) ch, x, y );

	// Draw rect for char bg
	ioctl(

		displayfd,
		DISP_IOCTL_DRAWFILLRECT,
		x,                        // x
		y,                        // y
		FONT_WIDTH,               // w
		FONT_HEIGHT,              // h
		textBgColor               // color
	);


	// Draw char
	ioctl(

		displayfd,
		DISP_IOCTL_DRAWBITMAP8,       // Assumes FONT_WIDTH == 8
		FONT + ( ch * FONT_HEIGHT ),
		x,
		y,
		FONT_HEIGHT,
		textColor
	);
}


// ____________________________________________________________________________________

void drawCursor ( void )
{
	uint x;
	uint y;

	//
	x = cursorCol * FONT_WIDTH;
	y = cursorRow * FONT_HEIGHT;

	//
	ioctl(

		displayfd,
		DISP_IOCTL_DRAWFILLRECT,
		x,                        // x
		y,                        // y
		1,                        // w
		FONT_HEIGHT,              // h
		cursorColor               // color
	);
}
