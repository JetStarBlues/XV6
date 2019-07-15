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

// Status bits
#define MOUSE_LEFT      0x01  // Bit indicating the left mouse button
#define MOUSE_RIGHT     0x02  // Bit indicating the right mouse button
#define MOUSE_MIDDLE    0x04  // Bit indicating the middle mouse button
#define MOUSE_XSIGN     0x10  // Sign bit for the x-axis
#define MOUSE_YSIGN     0x20  // Sign bit for the y-axis
#define MOUSE_XOVERFLOW 0x40  // Delta x is greater than 255 units
#define MOUSE_YOVERFLOW 0x80  // Delta y is greater than 255 units

//
#define MOUSECMD_ENABLE  0xF4  // Enable data reporting ?
#define MOUSECMD_DEFAULT 0xF6  // Use default settings
#define MOUSE_ACK        0xFA

//
#define MOUSE_PACKETSZ 3


static uchar mousePacket [ MOUSE_PACKETSZ ];
static int   bytesReceived;
static uchar leftBtn_prev  = 0;
static uchar rightBtn_prev = 0;
// static uchar middleBtn_prev = 0;

/*struct MousePacket {

	int   dx;
	int   dy;
	uchar leftBtn;
	uchar middleBtn;
	uchar rightBtn;
}*/


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

	// Tell controller to address the mouse
	mousewait_send();
	outb( PS2CTRL, PS2CMD_DEV2SEL );

	// Send the command
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
	uchar data;

	//
	bytesReceived = 0;


	// Enable the PS/2 controller's secondary device
	mousewait_send();
	outb( PS2CTRL, PS2CMD_DEV2EN );


	/* Receive an interrupt when the mouse state changes
	*/
	// Get current contents of the PS/2's configuration byte
	mousewait_send();
	outb( PS2CTRL, PS2CMD_RDCONFG );

	mousewait_recv();
	data = inb( PS2DATA );

	// Mark mouse interrupts as enabled
	mousewait_send();
	outb( PS2CTRL, PS2CMD_WRCONFG );

	mousewait_send();
	outb( PS2DATA, data |= PS2CONFG_DEV2EI );


	// Use default mouse settings
	mousewait_send();
	mousecmd( MOUSECMD_DEFAULT );

	// Activate mouse...
	mousewait_send();
	mousecmd( MOUSECMD_ENABLE );


	// Enable corresponding x86 interrupt
	ioapicenable( IRQ_MOUSE, 0 );
}

/* Mouse interrupt is sent when mouse moved or
   a mouse button changes state (press or release).

   The mouse sends only one byte with each interrupt.
   However, each mouse action is described with three bytes.
*/
void mouseintr ( void )
{
	uchar status;
	int   dx;
	int   dy;
	uchar leftBtn;
	uchar rightBtn;
	// uchar middleBtn;

	// cprintf( "!\n" );

	// No data available...
	if ( ( inb( PS2CTRL ) & PS2DINFULL ) == 0 )
	{
		return;
	}

	// Read data
	mousePacket[ bytesReceived ] = inb( PS2DATA );

	bytesReceived += 1;

	// Keep gathering packet
	if ( bytesReceived < MOUSE_PACKETSZ )
	{
		return;
	}


	// Process packet

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
		dx = ( int ) mousePacket[ 1 ];
		dy = ( int ) mousePacket[ 2 ];

		/* If negative, convert "9-bit signed" value into equivalent
		   "sizeof( int ) signed" value

		   Sign is 9th bit.
		   0x100 is largest negative number in "9-bit signed".

		   https://github.com/SerenityOS/serenity/blob/master/Kernel/Devices/PS2MouseDevice.cpp
		*/
		if ( status & MOUSE_XSIGN )
		{
			dx -= 0x100;
		}
		if ( status & MOUSE_YSIGN )
		{
			dy -= 0x100;
		}
	}

	// Get button status
	leftBtn  = ( status & MOUSE_LEFT )   ? 1 : 0;
	rightBtn = ( status & MOUSE_RIGHT )  ? 1 : 0;
	// middleBtn = ( status & MOUSE_MIDDLE ) ? 1 : 0;

	/*cprintf(

		"dx: %d, dy: %d, l: %d, r: %d, m: %d\n",
		dx, dy,
		leftBtn, rightBtn, middleBtn
	);*/


	// Call handler for onmousemove
	if ( dx || dy )
	{
		updateMouseCursor( dx, - dy );
	}

	// Call handler for onmousepress or onmouserelease
	/*
	if ( leftBtn != leftBtn_prev )
	{
		if ( leftBtn )
		{
			onMousePress();
		}
		else
		{
			onMouseRelease();
		}
	}
	*/


	// Temp test
	if ( leftBtn != leftBtn_prev )
	{
		if ( leftBtn_prev )
		{
			copyLine();
		}
	}
	if ( rightBtn != rightBtn_prev )
	{
		if ( rightBtn_prev )
		{
			pasteLine();
		}
	}


	// Save button status
	leftBtn_prev  = leftBtn;
	rightBtn_prev = rightBtn;
	// middleBtn_prev = middleBtn;
}
