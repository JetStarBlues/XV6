#include "types.h"
#include "x86.h"
#include "defs.h"
#include "traps.h"
#include "ps2.h"

/*
Based on:
	. cs3224 (fall 2016) Bonus Assignment
		. http://panda.moyix.net/~moyix/cs3224/fall16/bonus_hw/bonus_hw.html
		. https://github.com/moyix/xv6-public/blob/24cfc9d083d9fd332802f6ac7728d41b649e65a8/mouse.c

	. https://wiki.osdev.org/PS/2_Mouse
*/

/*
	Mouse sends three bytes:
		. byte0 - status
			0 - left button pressed
			1 - right button pressed
			2 - middle button pressed
			3 - always one
			4 - x sign
			5 - y sign
			6 - x overflow
			7 - y overflow
		. byte1 - x delta
		. byte2 - y delta
*/

// Constants used for talking to the PS/2 controller

#define MOUSE_ENABLEIRQ 0x02  // The enable IRQ 12 bit in the Compaq status byte

//
#define MOUSE_LEFT      0x01  // Bit indicating the left mouse button
#define MOUSE_RIGHT     0x02  // Bit indicating the right mouse button
#define MOUSE_MIDDLE    0x04  // Bit indicating the middle mouse button
#define MOUSE_XSIGN     0x10  // Sign bit for the x-axis
#define MOUSE_YSIGN     0x20  // Sign bit for the y-axis
#define MOUSE_XOVERFLOW 0x40  // Delta x is greater than 255 units
#define MOUSE_YOVERFLOW 0x80  // Delta y is greater than 255 units


#define MOUSE_ALWAYS_SET   0xC0      // This bit is always set to 1


#define MOUSE_ACK 0xFA

static void mousewait ( int is_read )
{
	uint tries = 100000;

	// Poll status
	while ( tries )
	{
		// If read, wait till input buffer is full
		if ( is_read )
		{
			if ( inb( PS2CTRL ) & PS2DINFULL )
			{
				return;
			}
		}
		// If write, wait till output buffer is empty
		else
		{
			if ( ( inb( PS2CTRL ) & PS2DOUTFULL ) == 0 )
			{
				return;
			}
		}

		tries -= 1;
	}

	cprintf( "mousewait: Tries expired\n" );
}

static void mousewait_recv ( void )
{
	mousewait( 1 );
}

static void mousewait_send ( void )
{
	mousewait( 0 );
}

void mousecmd ( uchar cmd )
{
	uchar data;

	// ??
	mousewait_send();
	outb( PS2CTRL, 0xD4 );

	mousewait_send();
	outb( PS2DATA, cmd );


	// Wait for command acknowledge...
	mousewait_recv();

	do
	{
		data = inb( PS2DATA );
	}
	while ( data != MOUSE_ACK );
}

void mouseinit ( void )
{
	// Enable ...
	mousewait_send();
	outb( PS2CTRL, 0xA8 );


	// Receive an interrupt when the mouse state changes

	// Get current contents of .. status byte
	mousewait_send();
	outb( PS2CTRL, 0x20 );

	mousewait_recv();
	data = inb( PS2DATA );

	// Mark mouse interrupts as enabled
	mousewait_send();
	outb( PS2CTRL, 0x60 );

	mousewait_send();
	outb( PS2DATA, data |= 0x02 );


	// Use default mouse settings
	mousewait_send();
	outb( ?, 0xF6 );

	// Activate mouse ...
	mousewait_send();
	outb( ?, 0xF4 );


	// Enable x86 interrupt
	ioapicenable( IRQ_MOUSE, 0 );
}

/* Mouse interrupt is sent when ??
*/
/* The mouse sends only one byte with each interrupt.
   However, each mouse action is described with three bytes.
*/

#define MOUSE_PACKETSZ 3
static uchar mousePacket [ MOUSE_PACKETSZ ];
static char bytesReceived = 0;

/*struct MousePacket {

	int dx;
	int dy;
	char leftBtn;
	char middleBtn;
	char rightBtn;
}*/

void mouseintr ( void )
{
	int dx, dy;

	cprintf( "Received mouse interrupt!\n" );

	// No data available...
	if ( ( inb( PS2CTRL ) & PS2DINFULL ) == 0 )
	{
		return;
	}

	// Read data
	if ( bytesReceived < MOUSE_PACKETSZ )
	{
		mousePacket[ bytesReceived ] = inb( PS2DATA );

		bytesReceived += 1;

		return;
	}
	// Process packet
	else
	{
		bytesReceived = 0;

		status = mousePacket[ 0 ];

		// Get dx, dy
		if ( ( status & MOUSE_XOVERFLOW ) || ( status & MOUSE_YOVERFLOW ) )
		{
			// Ignore...
			dx = 0;
			dy = 0;
		}
		else
		{
			dx = mousePacket[ 1 ];
			dy = mousePacket[ 2 ];

			if ( status & MOUSE_XSIGN )
			{
				dx = - dx;
			}
			if ( status & MOUSE_YSIGN )
			{
				dy = - dy;
			}
		}

		// Get button status
		leftBtn = ( status & MOUSE_LEFT )   ? 1 : 0;
		rightBtn = ( status & MOUSE_RIGHT )  ? 1 : 0;
		middleBtn = ( status & MOUSE_MIDDLE ) ? 1 : 0;

		// Debug
		cprintf(

			"dx: %d, dy: %d, left: %s, right: %s, middle: %s\n",
			dx, dy,
			leftBtn, rightBtn, middleBtn
		);
	}
}

