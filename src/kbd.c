#include "types.h"
#include "x86.h"
#include "defs.h"
#include "ps2.h"
#include "kbd.h"

int kbdgetc ( void )
{
	// ?
	static uint shift;

	// ?
	static uchar* charcode [ 4 ] = {

		normalmap,
		shiftmap,
		ctlmap,
		ctlmap
	};

	uint status,
	     data,
	     c;

	status = inb( PS2CTRL );

	// No data available...
	if ( ( status & PS2DINFULL ) == 0 )
	{
		return - 1;
	}

	// Read data
	data = inb( PS2DATA );

	// ?
	if ( data == 0xE0 )
	{
		shift |= E0ESC;

		return 0;
	}
	// ?
	else if ( data & 0x80 )
	{
		// Key released
		// data = ( shift & E0ESC ? data : data & 0x7F );
		data = ( shift & E0ESC ) ? data : ( data & 0x7F );  // JK... legibility

		shift &= ~ ( shiftcode[ data ] | E0ESC );

		return 0;
	}
	// ?
	else if ( shift & E0ESC )
	{
		// Last character was an E0 escape; or with 0x80
		data |= 0x80;

		shift &= ~ E0ESC;
	}

	shift |= shiftcode[ data ];
	shift ^= togglecode[ data ];

	c = charcode[ shift & ( CTL | SHIFT ) ][ data ];

	if ( shift & CAPSLOCK )
	{
		if ( 'a' <= c && c <= 'z' )
		{
			c += 'A' - 'a';
		}
		else if ( 'A' <= c && c <= 'Z' )
		{
			c += 'a' - 'A';
		}
	}

	return c;
}

/* Keyboard interrupt is sent when a key changes state
   (press or release)
*/
void kbdintr ( void )
{
	consoleintr( kbdgetc );
}
