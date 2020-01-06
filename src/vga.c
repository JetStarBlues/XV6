#include "types.h"
#include "defs.h"
#include "memlayout.h"
#include "x86.h"
#include "spinlock.h"
#include "vga.h"
#include "kfonts.h"


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


// TODO - should we be using locks ??


// General ___________________________________________________________________________

static struct
{
	/* This lock is used for ... ??
	*/
	struct spinlock lock;

} vga;

static int currentMode;


static void setTextMode                    ( void );
static void clearScreen_textMode           ( void );
static void updateMouseCursor_textMode     ( int, int );
static void updateMouseCursor_graphicsMode ( int, int );
static void vgaSetDefaultPalette_textMode  ( void );


void vgainit ( void )
{
	initlock( &vga.lock, "vga" );

	// Explicitly set to mode 0x03. Mainly so that we can use
	// a custom font from the get-go
	setTextMode();

	clearScreen_textMode();
}


// ___________________________________________________________________________________

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


// ___________________________________________________________________________________

static void setTextMode ( void )
{
	writeRegs( g_80x25_text );

	/* For some reason the font is erased when we switch modes,
	   so we need to write it back.
	   https://www.cs.uic.edu/bin/view/CS385fall14/Homework3
	*/
	writeFont( g_8x16_font, 16 );

	// Use default palette
	vgaSetDefaultPalette_textMode();

	// Clear the screen
	clearScreen_textMode();

	// Move the text cursor to (0,0)
	outb( CTRL, 14 );
	outb( DATA, 0  );  // send hi byte
	outb( CTRL, 15 );
	outb( DATA, 0  );  // send lo byte

	//
	currentMode = TXTMODE;
}

static void setGraphicsMode ( void )
{
	writeRegs( g_320x200x256 );

	currentMode = GFXMODE;
}

void vgaSetMode ( int sel )
{
	if ( sel == TXTMODE )
	{
		setTextMode();
	}
	else if ( sel == GFXMODE )
	{
		setGraphicsMode();
	}
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


// Palettes __________________________________________________________________________

void vgaSetPaletteColor ( int index, char r, char g, char b )
{
	outb( VGA_DAC_WRITE_INDEX, index );
	outb( VGA_DAC_DATA,        r ); 
	outb( VGA_DAC_DATA,        g ); 
	outb( VGA_DAC_DATA,        b ); 
}

// http://www.brackeen.com/vga/bitmaps.html
void vgaSetDefaultPalette ( void )
{
	int  i;
	char r, g, b;

	outb( VGA_DAC_WRITE_INDEX, 0 );

	for ( i = 0; i < 768; i += 3 )
	{
		/*
		r = vga256_18bit_custom[ i     ];
		g = vga256_18bit_custom[ i + 1 ];
		b = vga256_18bit_custom[ i + 2 ];*/

		r = vga256_18bit_default[ i     ];
		g = vga256_18bit_default[ i + 1 ];
		b = vga256_18bit_default[ i + 2 ];

		outb( VGA_DAC_DATA, r ); 
		outb( VGA_DAC_DATA, g ); 
		outb( VGA_DAC_DATA, b ); 
	}
}


/*static void vgaSetPaletteColor_textMode ( int index, char r, char g, char b )
{
	outb( VGA_DAC_WRITE_INDEX, EGAtoVGA[ index ] );
	outb( VGA_DAC_DATA,        r );
	outb( VGA_DAC_DATA,        g );
	outb( VGA_DAC_DATA,        b );
}*/

/* Restore default palette of text mode
   https://forum.osdev.org/viewtopic.php?f=1&t=23753&p=192800#p192800
*/
static void vgaSetDefaultPalette_textMode ( void )
{
	int  egaIdx;
	int  vgaIdx;
	int  palIdx;
	char r, g, b;

	for ( egaIdx = 0; egaIdx < 16; egaIdx += 1 )
	{
		// Get expected EGA color
		palIdx = egaIdx * 3;

		r = vga256_18bit_default[ palIdx     ];
		g = vga256_18bit_default[ palIdx + 1 ];
		b = vga256_18bit_default[ palIdx + 2 ];


		// Place it in expected VGA slot
		vgaIdx = EGAtoVGA[ egaIdx ];

		vgaSetPaletteColor( vgaIdx, r, g, b );
	}
}


/* Turn byte channels (0xRRGGBB) into 6 bit channels
   by dropping the lowest 2 bits.
   https://en.wikipedia.org/wiki/List_of_monochrome_and_RGB_palettes#18-bit_RGB
*/
/*void convert24To18bit ( int color24, int* r, int* g, int* b )
{
	*r = ( ( color24 & 0xff0000 ) >> 16 ) >> 2;
	*g = ( ( color24 & 0x00ff00 ) >> 8  ) >> 2;
	*b = ( ( color24 & 0x0000ff )       ) >> 2;
}*/


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

#define NTABSPACES 3

static ushort* textbuffer = ( ushort* ) P2V( TXTBUFFER );

static int selectioncolor_textMode =       TXTCOLOR( TCYAN,   TMAGENTA );
static int textcolor_textMode      =       TXTCOLOR( TYELLOW, TRED );
static int clearchar_textMode      = ' ' | TXTCOLOR( TYELLOW, TRED );

static int mouseX_textMode = WIDTH_TXTMODE  / 2;  // middle of screen
static int mouseY_textMode = HEIGHT_TXTMODE / 2;

static int mouseHasPreviouslyMoved_textMode = 0;
static int mousePosPrev_textMode;

static char selectingText;  // ...


static void clearScreen_textMode ( void )
{
	int i;

	// Most efficient, but only clears to black
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

/*static void setTextCursorPos ( ushort pos )
{
	outb( CTRL, 14 );
	outb( DATA, pos >> 8 );  // send hi byte
	outb( CTRL, 15 );
	outb( DATA, pos );       // send lo byte
}*/


//
void vgaputc ( int c )
{
	ushort pos;
	int    i;

	if ( currentMode != TXTMODE )
	{
		return;
	}

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
	// Tab
	else if ( c == '\t' )
	{
		for ( i = 0; i < NTABSPACES; i += 1 )
		{
			textbuffer[ pos ] = clearchar_textMode;

			pos += 1;
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


// Draws mouse by inverting colors
static void drawMouseCursor_textMode ( int pos )
{
	ushort curBgColor;
	ushort curFgColor;
	ushort curChar;

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
		/* Don't restore a non-existent mouse cursor.
		*/
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
}

/* There is a minor glitch:
     When the mouse is in a region that vgaputc overwrites
     (text cursor or text cursor + 1 ), vgaputc erases the mouse visually...
     When this update function is later called, its attempt
     to restore the mouse's previous location ends up drawing
     the mouse again...
     One solution is for vgaputc to write the character at that
     location using inverted colors (and thus preserve the visual mouse)
   For now, we ignore the glitch (to keep the two functions isolated).
*/
static void updateMouseCursor_textMode ( int dx, int dy )
{
	int    x;
	int    y;
	int    pos;

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
	x = mouseX_textMode / COLWIDTH;
	y = mouseY_textMode / ROWHEIGHT;

	pos = y * NCOLS + x;


	// Draw mouse cursor in new position
	if ( ! selectingText )
	{
		drawMouseCursor_textMode( pos );
	}


	// Save present as past
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


static void vgaWritePixel ( int x, int y, int colorIdx )
{
	int off;

	// Should check bounds. Ignoring for speed.

	off = WIDTH_GFXMODE * y + x;

	gfxbuffer[ off ] = colorIdx;
}

#if 0
	void vgaBlit ( uchar* src )
	{
		memcpy( gfxbuffer, src, WIDTHxHEIGHT_GFXMODE );
	}

	void vgaFillRect ( int x, int y, int w, int h, int colorIdx )
	{
		int off;
		int x2;
		int y2;

		// Should check bounds. Ignoring for speed.

		// If screen dimensions, memset
		if ( ( x == 0 ) && ( y == 0 ) && ( w == WIDTH_GFXMODE ) && ( h == HEIGHT_GFXMODE ) )
		{
			memset( gfxbuffer, colorIdx, WIDTHxHEIGHT_GFXMODE );
		}

		// Otherwise do math...
		else
		{
			for ( y2 = y; y2 < ( y + h ); y2 += 1 )
			{
				off = WIDTH_GFXMODE * y2 + x;

				for ( x2 = x; x2 < ( x + w ); x2 += 1 )
				{
					gfxbuffer[ off ] = colorIdx;

					off += 1;
				}
			}
		}
	}

	/* Draws a bitmap of width 8, height 'h'.
	   Encoding:
	    . Each byte represents a row.
	    . A set bit represents a pixel.
	*/
	void vgaDrawBitmap8 ( uchar* bitmap, int x, int y, int h, int colorIdx )
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

			off = WIDTH_GFXMODE * y2 + x;

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
	}
#endif


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

#if 0
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
		vgaSetPaletteColor( yellow, 63, 63, 0 );  // yes

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
#endif


// Mouse selection (text mode) _______________________________________________________

// http://panda.moyix.net/~moyix/cs3224/fall16/bonus_hw/bonus_hw.html
#define COPYBUFSZ 100
static uchar copypastebuffer [ COPYBUFSZ ];
static int   copypastebuffer_idx = 0;

static int  selectionStartPos;
static int  selectionEndPos;
static char selectingText     = 0;
static int  selection_prevpos = 0;
static int  selection_prevn   = 0;


void markSelectionStart ( void )
{
	if ( currentMode != TXTMODE )
	{
		return;
	}

	selectionStartPos = mousePosPrev_textMode;

	selectingText = 1;
}

void markSelectionEnd ( void )
{
	if ( currentMode != TXTMODE )
	{
		return;
	}

	// Calculate the selection's start and end positions
	if ( mousePosPrev_textMode > selectionStartPos )
	{
		selectionEndPos = mousePosPrev_textMode;
	}
	else
	{
		// If selected leftwards, swap start and end
		selectionEndPos   = selectionStartPos + 1;  // inclusive range...
		selectionStartPos = mousePosPrev_textMode;
	}


	selectingText = 0;

	/* Mouse cursor is currently inside selection.
	   We want to prevent it from being "restored" visually
	   when the mouse is drawn.
	*/
	mouseHasPreviouslyMoved_textMode = 0;
}

void highlightSelection ( void )
{
	int    pos;
	int    endpos;
	int    n;
	int    i;
	ushort curChar;

	if ( ! selectingText )
	{
		return;
	}


	// Clear previous highlight
	pos = selection_prevpos;

	for ( i = 0; i < selection_prevn; i += 1 )
	{
		curChar = textbuffer[ pos ] & 0x00FF;

		textbuffer[ pos ] = textcolor_textMode | curChar;

		pos += 1;
	}


	// Calculate start and size of new highlight
	endpos = mousePosPrev_textMode;

	if ( endpos >= selectionStartPos )
	{
		pos = selectionStartPos;

		n = endpos - selectionStartPos;
	}
	else
	{
		pos = endpos;

		n = selectionStartPos + 1 - endpos;  // inclusive range...
	}

	n = MIN( n, NCOLSxNROWS - pos );


	// Save present as past
	selection_prevpos = pos;
	selection_prevn   = n;


	// Create new highlight
	for ( i = 0; i < n; i += 1 )
	{
		curChar = textbuffer[ pos ] & 0x00FF;

		textbuffer[ pos ] = selectioncolor_textMode | curChar;

		pos += 1;
	}
}

void copySelection ( void )
{
	int   i;
	int   n;
	int   pos;

	if ( currentMode != TXTMODE )
	{
		return;
	}


	// Clear buffer
	memset(

		copypastebuffer,
		0,
		sizeof( uchar ) * COPYBUFSZ
	);


	// Update buffer
	pos = selectionStartPos;

	n = selectionEndPos - selectionStartPos;

	n = MIN( n, COPYBUFSZ );

	for ( i = 0; i < n; i += 1 )
	{
		copypastebuffer[ i ] = ( uchar ) ( textbuffer[ pos ] & 0x00FF );

		pos += 1;
	}
}

static int copypastebuffer_getc ( void )
{
	uchar c;

	// Reached end of buffer
	if ( copypastebuffer_idx > COPYBUFSZ - 1 )
	{
		copypastebuffer_idx = 0;

		return - 1;
	}

	c = copypastebuffer[ copypastebuffer_idx ];

	// Reached end of valid contents...
	// if ( c == 0 )
	if ( c < 0x20 || c > 0x7E )  // printable characters
	{
		copypastebuffer_idx = 0;

		return - 1;
	}
	else
	{
		copypastebuffer_idx += 1;

		return c;
	}
}

void pasteSelection ()
{
	if ( currentMode != TXTMODE )
	{
		return;
	}

	consoleintr( copypastebuffer_getc );
}
