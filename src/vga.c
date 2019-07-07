#include "types.h"
#include "defs.h"
// #include "param.h"
// #include "traps.h"
// #include "spinlock.h"
// #include "sleeplock.h"
// #include "fs.h"
// #include "file.h"
#include "memlayout.h"
// #include "mmu.h"
// #include "proc.h"
#include "x86.h"

/*
Based on:
	http://www.jamesmolloy.co.uk/tutorial_html/3.-The%20Screen.html
*/

/*
Text mode
	. http://www.jamesmolloy.co.uk/tutorial_html/3.-The%20Screen.html

	. The framebuffer is an array of 16-bit words, with each value
	  representing the display of one character.

	. The offset from the start of the buffer specifying a character
	  at position (x, y) or (col, row) is:

		  x + (   y * NCOLS )
		col + ( row * NCOLS )

	. The 16 bits are used as follows:
		15..12 - background color
		11..8  - foreground color
		 7..0  - ascii code
	
	. 16 possible colors, see below.

	. Registers - control (0x3d4), data (0x3d5)
*/

// Text mode ____________________________________

#define TXTBUFFER 0xb8000  // physical address

#define NCOLS       80
#define NROWS       25
#define NCOLSxNROWS 2000  // NCOLS x NROWS

#define CTRL 0x3d4
#define DATA 0x3d5

#define TBLACK         0
#define TBLUE          1
#define TGREEN         2
#define TCYAN          3
#define TRED           4
#define TMAGENTA       5
#define TBROWN         6
#define TLIGHT_GREY    7
#define TDARK_GREY     8
#define TLIGHT_BLUE    9
#define TLIGHT_GREEN   10
#define TLIGHT_CYAN    11
#define TLIGHT_RED     12
#define TLIGHT_MAGNETA 13
#define TYELLOW        14
#define TWHITE         15

#define BACKSPACE 0x100

// txtcolor( bg, fg ) = ( ( bg << 4 ) | fg ) << 8
#define TXTCOLOR( BG, FG ) ( ( ( ( BG ) << 4 ) | ( FG ) ) << 8 )


void vgaputc ( int c )
{
	static ushort* textbuffer = ( ushort* ) P2V( TXTBUFFER );

	int pos;

	// Get current cursor position
	outb( CTRL, 14 );
	pos = inb( DATA ) << 8;  // get hi byte

	outb( CTRL, 15 );
	pos |= inb( DATA );      // get lo byte


	/* Calculate new cursor position and if applicable,
	   draw the character.
	*/
	// Newline
	if ( c == '\n' )
	{
		pos += NCOLS - ( pos % NCOLS );
	}
	// Backspace
	else if ( c == BACKSPACE )
	{
		if ( pos > 0 )
		{
			pos -= 1;
		}
	}
	// Everything else
	else
	{
		// Draw the character by placing it in the buffer
		textbuffer[ pos ] = ( c & 0xff ) | TXTCOLOR( TYELLOW, TRED );

		pos += 1;
	}

	if ( pos < 0 || pos > NCOLSxNROWS )
	{
		panic( "vgaputc: invalid pos" );
	}


	// If moved past last row, scroll up
	if ( ( pos / NCOLS ) >= NROWS )
	{
		// Move the current contents up a row
		memmove(

			textbuffer,
			textbuffer + NCOLS,
			sizeof( textbuffer[ 0 ] ) * ( NROWS - 1 ) * NCOLS
		);

		// Move cursor to last row
		pos -= NCOLS;

		// Clear the last row
		memset(

			textbuffer + pos,
			0,
			sizeof( textbuffer[ 0 ] ) * ( NCOLSxNROWS - pos )
		);
	}


	// Move the cursor
	outb( CTRL, 14 );
	outb( DATA, pos >> 8 );  // send hi byte
	outb( CTRL, 15 );
	outb( DATA, pos );       // send lo byte


	// Blank area where cursor will appear (cursor is drawn by terminal)
	textbuffer[ pos ] = ' ' | TXTCOLOR( TYELLOW, TRED );
}


/*
Graphics mode
	. 
*/

// Graphics mode _________________________________

#define GFXBUFFER 0xa0000  // physical address

