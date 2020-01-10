#include "types.h"
#include "user.h"
#include "fonts.h"
#include "GFX.h"
#include "GFXtext.h"

/* Simple interface...

   Currently exists to facilitate kEditor.
   Refine API if intend to use elsewhere.

   Caller is responsible for:
   . keeping track of cursor position
   . calling 'GFX_init' before use, and 'GFX_exit' after use

*/

// ____________________________________________________________________________________

// Font
#define FONT        g_8x11_font__Gohu
// #define FONT_BOLD   g_8x11_font__Gohu_bold
#define FONT_WIDTH  8
#define FONT_HEIGHT 11

// Dimensions in characters
static int nRows = SCREEN_HEIGHT / FONT_HEIGHT;
static int nCols = SCREEN_WIDTH  / FONT_WIDTH;

// Cursor position
static int cursorRow;
static int cursorCol;

// Default colors (standard 256-color VGA palette)
static uchar textColor   = 24;   // grey
static uchar textBgColor = 15;   // white
static uchar cursorColor = 112;  // brown

// Screen framebuffer
static uchar* gfxbuffer = ( uchar* ) GFXBUFFER;


// ____________________________________________________________________________________

void GFXText_setCursorPosition ( int row, int col )
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

void GFXText_getCursorPosition ( int* rowPtr, int* colPtr )
{
	*rowPtr = cursorRow;
	*colPtr = cursorCol;
}

void GFXText_getDimensions ( int* nRowsPtr, int* nColsPtr )
{
	*nRowsPtr = nRows;
	*nColsPtr = nCols;
}


// ____________________________________________________________________________________

void GFXText_setTextColor ( uchar color )
{
	textColor = color;
}
void GFXText_setTextBgColor ( uchar color )
{
	textBgColor = color;
}
void GFXText_setCursorColor ( uchar color )
{
	cursorColor = color;
}
void GFXText_invertTextColors ( void )
{
	uchar tmp;

	tmp         = textColor;
	textColor   = textBgColor;
	textBgColor = tmp;
}


// ____________________________________________________________________________________

// Simple, terrible performance.

/* Assumes the following bitmap encoding:
    . Each byte represents a row
    . A set bit represents a pixel
*/
static void drawFontChar ( uchar ch, int x, int y )
{
	uchar* bitmap;
	uchar  bitmapRow;
	uchar  pixelMask;
	int    i;
	int    j;
	int    y2;
	int    off;

	// Should check bounds. Ignoring for speed.

	//
	y2 = y;

	// Base location of character's bitmap
	bitmap = ( uchar* ) FONT + ( ch * FONT_HEIGHT );


	// For each row in the character...
	for ( j = 0; j < FONT_HEIGHT; j += 1 )
	{
		bitmapRow = bitmap[ j ];

		off = SCREEN_WIDTH * y2 + x;

		// For each bit in the current row...
		for ( i = FONT_WIDTH - 1; i >= 0; i -= 1 )
		{
			pixelMask = ( uchar ) ( 1 << i );

			// If the bit is set, draw a "font" pixel
			if ( ( bitmapRow & pixelMask ) != 0 )
			{
				gfxbuffer[ off ] = textColor;
			}
			// Otherwise, draw a "background" pixel
			else
			{
				gfxbuffer[ off ] = textBgColor;
			}

			off += 1;
		}

		y2 += 1;
	}
}

void GFXText_printChar ( uchar ch )
{
	int x;
	int y;

	//
	x = cursorCol * FONT_WIDTH;
	y = cursorRow * FONT_HEIGHT;

	// printf( 1, "%d -> %d, %d\n", ( int ) ch, x, y );

	drawFontChar( ch, x, y );
}


// ____________________________________________________________________________________

void GFXText_drawCursor ( void )
{
	int x;
	int y;

	//
	x = cursorCol * FONT_WIDTH;
	y = cursorRow * FONT_HEIGHT;

	//
	GFX_fillRect(

		x,            // x
		y,            // y
		1,            // w
		FONT_HEIGHT,  // h
		cursorColor   // color
	);
}


// ____________________________________________________________________________________

void GFXText_clearScreen ( void )
{
	GFX_clearScreen( textBgColor );
}
