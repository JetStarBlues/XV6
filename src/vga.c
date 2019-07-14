#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "x86.h"
#include "vga.h"

/*
Text mode code is based on:
	http://www.jamesmolloy.co.uk/tutorial_html/3.-The%20Screen.html
	https://wiki.osdev.org/Drawing_In_Protected_Mode

Code to control the VGA (including switching between modes) is by:
	Chris Giese <geezer@execpc.com>	http://my.execpc.com/~geezer
	 . https://files.osdev.org/mirrors/geezer/osd/graphics/index.htm
	 . https://files.osdev.org/mirrors/geezer/osd/graphics/modes.c

Code to integrate graphics mode with xv6 userspace is based on:
	https://github.com/sam46/xv6
	 . vga.c
	 . display.c
	 . imshow.c
*/


// General ___________________________________________________________________________

#define TXTMODE 1
#define GFXMODE 3

static int currentMode;

static void clearScreen_textMode ();
void        setTextMode          ( void );
static void updateMouseCursor_textMode ( int, int );
static void updateMouseCursor_graphicsMode ( int, int );

void vgainit ()
{
	// Explicitly set to mode 0x03. Mainly so that we can use
	// a custom font from the get-go
	setTextMode();

	clearScreen_textMode();
}

static void writeRegs ( char* regs )
{
	int i;

	/* Write Miscellaneous reg */
	outb( VGA_MISC_WRITE, *regs );
	regs += 1;

	/* Write Sequencer regs */
	for ( i = 0; i < VGA_NUM_SEQ_REGS; i += 1 )
	{
		outb( VGA_SEQ_INDEX, i );
		outb( VGA_SEQ_DATA,  *regs );
		regs += 1;
	}

	/* Unlock CRTC registers */
	outb( VGA_CRTC_INDEX, 0x03 );
	outb( VGA_CRTC_DATA,  inb( VGA_CRTC_DATA ) | 0x80 );
	outb( VGA_CRTC_INDEX, 0x11 );
	outb( VGA_CRTC_DATA,  inb( VGA_CRTC_DATA ) & ( ~ 0x80 ) );

	/* Make sure they remain unlocked */
	regs[ 0x03 ] |= 0x80;
	regs[ 0x11 ] &= ( ~ 0x80 );

	/* Write CRTC regs */
	for ( i = 0; i < VGA_NUM_CRTC_REGS; i += 1 )
	{
		outb( VGA_CRTC_INDEX, i );
		outb( VGA_CRTC_DATA,  *regs );
		regs += 1;
	}

	/* Write Graphics Controller regs */
	for ( i = 0; i < VGA_NUM_GC_REGS; i += 1 )
	{
		outb( VGA_GC_INDEX, i );
		outb( VGA_GC_DATA,  *regs );
		regs += 1;
	}

	/* Write Attribute Controller regs */
	for ( i = 0; i < VGA_NUM_AC_REGS; i += 1 )
	{
		inb( VGA_INSTAT_READ );
		outb( VGA_AC_INDEX, i );
		outb( VGA_AC_WRITE, *regs );
		regs += 1;
	}

	/* Lock 16-color palette and unblank display */
	inb( VGA_INSTAT_READ );
	outb( VGA_AC_INDEX, 0x20 );
}

static void setPlane ( int p )
{
	char pmask;

	p     &= 3;
	pmask  = 1 << p;

	/* Set read plane */
	outb( VGA_GC_INDEX, 4 );
	outb( VGA_GC_DATA, p );

	/* Set write plane */
	outb( VGA_SEQ_INDEX, 2 );
	outb( VGA_SEQ_DATA, pmask );
}

static void writeFont ( char* font, int fontheight )
{
	char seq2, seq4, gc4, gc5, gc6;
	int  i;
	int  offset;

	/* Save registers
	   setPlane() modifies GC 4 and SEQ 2, so save them as well
	*/
	outb( VGA_SEQ_INDEX, 2 );
	seq2 = inb( VGA_SEQ_DATA );

	outb( VGA_SEQ_INDEX, 4 );
	seq4 = inb( VGA_SEQ_DATA );

	/* Turn off even-odd addressing (set flat addressing)
	   assume: chain-4 addressing already off
	*/
	outb( VGA_SEQ_DATA, seq4 | 0x04 );

	outb( VGA_GC_INDEX, 4 );
	gc4 = inb( VGA_GC_DATA );

	outb( VGA_GC_INDEX, 5 );
	gc5 = inb( VGA_GC_DATA );

	/* Turn off even-odd addressing */
	outb( VGA_GC_DATA, gc5 & ( ~ 0x10 ) );

	outb( VGA_GC_INDEX, 6 );
	gc6 = inb( VGA_GC_DATA );

	/* Turn off even-odd addressing */
	outb( VGA_GC_DATA, gc6 & ( ~ 0x02 ) );

	/* Write font to plane P4 */
	setPlane( 2 );

	/* Write font */
	for ( i = 0; i < 256; i += 1 )
	{
		offset = i * 32;  // ?? Maybe maximum height a VGA font can be is 32

		memmove(

			( uchar* ) ( P2V( TXTBUFFER + offset ) ),  // dst
			font,                                      // src
			fontheight
		);

		font += fontheight;
	}

	/* Restore registers */
	outb( VGA_SEQ_INDEX, 2 );
	outb( VGA_SEQ_DATA,  seq2 );
	outb( VGA_SEQ_INDEX, 4 );
	outb( VGA_SEQ_DATA,  seq4 );
	outb( VGA_GC_INDEX,  4 );
	outb( VGA_GC_DATA,   gc4 );
	outb( VGA_GC_INDEX,  5 );
	outb( VGA_GC_DATA,   gc5 );
	outb( VGA_GC_INDEX,  6 );
	outb( VGA_GC_DATA,   gc6 );
}

void setTextMode ( void )
{
	writeRegs( g_80x25_text );

	/* For some reason the font is erased when we switch modes,
	   so we need to write it back.
	   https://www.cs.uic.edu/bin/view/CS385fall14/Homework3
	*/
	writeFont( g_8x16_font, 16 );

	// Clear screen
	clearScreen_textMode();

	//
	currentMode = TXTMODE;
}

void setGraphicsMode ( void )
{
	writeRegs( g_320x200x256 );

	currentMode = GFXMODE;
}

void updateMouseCursor ( int dx, int dy )
{
	if ( currentMode == TXTMODE )
	{
		updateMouseCursor_textMode( dx, dy );
	}
	else if ( currentMode == GFXMODE )
	{
		updateMouseCursor_graphicsMode( dx, dy );
	}
}


// Text mode _________________________________________________________________________

/*
Text mode (VGA mode 0x03)
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

	. Registers - control (0x3D4), data (0x3D5)
*/


/* Macro generates upper color byte:
    txtcolor( bg, fg ) = ( ( bg << 4 ) | fg ) << 8
*/
#define TXTCOLOR( BG, FG ) ( ( ( ( BG ) << 4 ) | ( FG ) ) << 8 )


static ushort* textbuffer     = ( ushort* ) P2V( TXTBUFFER );
static int textcolor_textMode =       TXTCOLOR( TYELLOW, TRED );
static int clearchar_textMode = ' ' | TXTCOLOR( TYELLOW, TRED );

static int mouseX_textMode = WIDTH_TXTMODE  / 2;  // middle of screen
static int mouseY_textMode = HEIGHT_TXTMODE / 2;

static int mouseHasPreviouslyMoved_textMode = 0;
static int mousePosPrev_textMode;


static void clearScreen_textMode ()
{
	int i;

	// Most efficient?, but only clears to black
	/*memset(

		textbuffer,
		0,
		sizeof( textbuffer[ 0 ] ) * NCOLSxNROWS
	);*/

	for ( i = 0; i < NCOLSxNROWS; i += 1 )
	{
		textbuffer[ i ] = clearchar_textMode;
	}
}

void vgaputc ( int c )
{
	// static ushort* textbuffer = ( ushort* ) P2V( TXTBUFFER );

	int pos;
	int i;

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
		textbuffer[ pos ] = ( c & 0xff ) | textcolor_textMode;

		pos += 1;
	}

	// Constrain position
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
		/*memset(

			textbuffer + pos,
			0,
			sizeof( textbuffer[ 0 ] ) * ( NCOLSxNROWS - pos )
		);*/
		for ( i = pos; i < NCOLSxNROWS; i += 1 )
		{
			textbuffer[ i ] = clearchar_textMode;
		}
	}


	// Move the cursor
	outb( CTRL, 14 );
	outb( DATA, pos >> 8 );  // send hi byte
	outb( CTRL, 15 );
	outb( DATA, pos );       // send lo byte


	// Blank area where cursor will appear (cursor is drawn by terminal)
	textbuffer[ pos ] = ' ' | textcolor_textMode;
}


static void updateMouseCursor_textMode ( int dx, int dy )
{
	int    x;
	int    y;
	int    pos;
	ushort curBgColor;
	ushort curFgColor;
	ushort curChar;


	// Update position
	mouseX_textMode += dx;
	mouseY_textMode += dy;

	// Constrain position
	if ( mouseX_textMode > WIDTH_TXTMODE )
	{
		mouseX_textMode = WIDTH_TXTMODE - 1;
	}
	else if ( mouseX_textMode < 0 )
	{
		mouseX_textMode = 0;
	}
	if ( mouseY_textMode > HEIGHT_TXTMODE )
	{
		mouseY_textMode = HEIGHT_TXTMODE - 1;
	}
	else if ( mouseY_textMode < 0 )
	{
		mouseY_textMode = 0;
	}

	// Convert to text mode coordinates (floor division)
	x = mouseX_textMode / ( WIDTH_TXTMODE  / NCOLS );
	y = mouseY_textMode / ( HEIGHT_TXTMODE / NROWS );

	pos = y * NCOLS + x;


	// Draw by inverting colors ---

	// Restore previous position's colors
	if ( mouseHasPreviouslyMoved_textMode )
	{
		curBgColor = textbuffer[ mousePosPrev_textMode ] & 0xF000;
		curFgColor = textbuffer[ mousePosPrev_textMode ] & 0x0F00;
		curChar    = textbuffer[ mousePosPrev_textMode ] & 0x00FF;

		textbuffer[ mousePosPrev_textMode ] = (

			( curFgColor << 4 ) |
			( curBgColor >> 4 ) |
			  curChar
		);
	}
	else
	{
		mouseHasPreviouslyMoved_textMode = 1;
	}

	// Invert new position's colors
	curBgColor = textbuffer[ pos ] & 0xF000;
	curFgColor = textbuffer[ pos ] & 0x0F00;
	curChar    = textbuffer[ pos ] & 0x00FF;

	textbuffer[ pos ] = (

		( curFgColor << 4 ) |
		( curBgColor >> 4 ) |
		  curChar
	);

	//
	mousePosPrev_textMode = pos;
}


// Graphics mode _____________________________________________________________________

/*
Graphics mode (VGA mode 0x13)

	. 320x200, 256 color

	. Two elements:
		. graphics - pixel map
		. palette  - color map

	. The graphics buffer is an array of size 320x200
	. Each pixel is represented by a byte (8 bits).
	  The byte is used to index the palette

	. The palette holds 256 colors
	. Each color is represented by 18 bits - 6 bits red, 6 bits green, 6 bits blue
*/


static uchar* gfxbuffer = ( uchar* ) P2V( GFXBUFFER );

static int mouseX_gfxMode = WIDTH_GFXMODE  / 2;
static int mouseY_gfxMode = HEIGHT_GFXMODE / 2;


void vgaWritePixel ( int x, int y, int c )
{
	int off;

	off = WIDTH_GFXMODE * y + x;

	gfxbuffer[ off ] = c;
}


void vgaSetPalette ( int index, char r, char g, char b )
{
	outb( VGA_DAC_WRITE_INDEX, index );
	outb( VGA_DAC_DATA,        r ); 
	outb( VGA_DAC_DATA,        g ); 
	outb( VGA_DAC_DATA,        b ); 
}

void vgaSetDefaultPalette ()
{
	int index;
	char r, g, b;

	for ( index = 0; index < 256; index += 1 )
	{
		/*
		r = vga256_18bit_custom[ index     ];
		g = vga256_18bit_custom[ index + 1 ];
		b = vga256_18bit_custom[ index + 2 ];*/

		r = vga256_18bit_default[ index     ];
		g = vga256_18bit_default[ index + 1 ];
		b = vga256_18bit_default[ index + 2 ];

		vgaSetPalette( index, r, g, b );
	}
}

/* Turn byte channels (0xRRGGBB) into 6 bit channels
   by dropping the lowest 2 bits.
   https://en.wikipedia.org/wiki/List_of_monochrome_and_RGB_palettes#18-bit_RGB
*/
void convert24To18bit ( int color24, int* r, int* g, int* b )
{
	*r = ( ( color24 & 0xff0000 ) >> 16 ) >> 2;
	*g = ( ( color24 & 0x00ff00 ) >> 8  ) >> 2;
	*b = ( ( color24 & 0x0000ff )       ) >> 2;
}


static void updateMouseCursor_graphicsMode ( int dx, int dy )
{
	// Color indices in standard VGA 256 palette
	const int black = 0;
	const int white = 15;

	// Update position
	mouseX_gfxMode += dx;
	mouseY_gfxMode += dy;

	// Constrain position
	if ( mouseX_gfxMode > WIDTH_GFXMODE )
	{
		mouseX_gfxMode = WIDTH_GFXMODE - 1;
	}
	else if ( mouseX_gfxMode < 0 )
	{
		mouseX_gfxMode = 0;
	}
	if ( mouseY_gfxMode > HEIGHT_GFXMODE )
	{
		mouseY_gfxMode = HEIGHT_GFXMODE - 1;
	}
	else if ( mouseY_gfxMode < 0 )
	{
		mouseY_gfxMode = 0;
	}

	// Draw a '+'
	vgaWritePixel( mouseX_gfxMode, mouseY_gfxMode, white );

	if ( mouseX_gfxMode < WIDTH_GFXMODE - 1 )
	{
		vgaWritePixel( mouseX_gfxMode + 1, mouseY_gfxMode,     black );
	}
	if ( mouseX_gfxMode > 1 )
	{
		vgaWritePixel( mouseX_gfxMode - 1, mouseY_gfxMode,     black );
	}
	if ( mouseY_gfxMode < HEIGHT_GFXMODE - 1 )
	{
		vgaWritePixel( mouseX_gfxMode,     mouseY_gfxMode + 1, black );
	}
	if ( mouseY_gfxMode > 1 )
	{
		vgaWritePixel( mouseX_gfxMode,     mouseY_gfxMode - 1, black );
	}
}


// Tests _____________________________________________________________________________

static void drawX ( void )
{
	// Color indices in standard VGA 256 palette
	// const int black  = 0;
	// const int blue    = 1;
	const int green   = 2;
	const int cyan    = 3;
	const int red     = 4;
	const int magenta = 13;
	const int yellow  = 14;

	int x, y;

	// Can I get a brighter yellow?
	vgaSetPalette( yellow, 63, 63, 0 );  // yes

	/* Clear screen */
	for ( y = 0; y < HEIGHT_GFXMODE; y += 1 )
	{
		for ( x = 0; x < WIDTH_GFXMODE; x += 1 )
		{
			vgaWritePixel( x, y, yellow );
		}
	}

	/* Draw 2-color X */
	for ( y = 0; y < HEIGHT_GFXMODE; y += 1 )
	{
		vgaWritePixel( ( WIDTH_GFXMODE - HEIGHT_GFXMODE ) / 2 + y, y, magenta );
		vgaWritePixel( ( HEIGHT_GFXMODE + WIDTH_GFXMODE ) / 2 - y, y, cyan );
	}

	/* Draw 2-color + */
	for ( x = 0; x < WIDTH_GFXMODE; x += 1 )
	{
		vgaWritePixel( x, HEIGHT_GFXMODE / 2, red );
	}
	for ( y = 0; y < HEIGHT_GFXMODE; y += 1 )
	{
		vgaWritePixel( WIDTH_GFXMODE / 2, y, green );
	}
}

void demoGraphics ( void )
{
	setGraphicsMode();

	drawX();
}
