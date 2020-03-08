#include "types.h"
#include "x86.h"
#include "defs.h"
#include "traps.h"
#include "ps2.h"
#include "mouse.h"

/*
Based on:
	. cs3224 (fall 2016) Bonus Assignment
		. http://panda.moyix.net/~moyix/cs3224/fall16/bonus_hw/bonus_hw.html
		. https://github.com/moyix/xv6-public/blob/bonus_hw/mouse.c

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


//
static uchar mousePacket [ MOUSE_PACKETSZ ];
static int   bytesReceived;

//
static struct mouseStatus mStatus;


// _____________________________________________________________________________

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


static void mousecmd ( uchar cmd )
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


// _____________________________________________________________________________

static void mouseinit_ps2 ( void )
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


// _____________________________________________________________________________

void mouseinit ( void )
{
	// Do we need locks?

	// devsw[ MOUSE ].ioctl = mouseioctl;  // userspace access

	mouseinit_ps2();
}


// _____________________________________________________________________________

/* Mouse interrupt is sent when mouse moved or
   a mouse button changes state (press or release).

   The mouse sends only one byte with each interrupt.
   However, each mouse action is described with three bytes.
*/
void mouseintr ( void )
{
	uchar statusByte;


	// Get packet - - - - - - - - - - - - - - -

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


	// Process packet - - - - - - - - - - - - -

	bytesReceived = 0;

	statusByte = mousePacket[ 0 ];


	// Get deltaX and deltaY
	if ( ( statusByte & MOUSE_XOVERFLOW ) || ( statusByte & MOUSE_YOVERFLOW ) )
	{
		// Ignore...
		mStatus.deltaX = 0;
		mStatus.deltaY = 0;
	}
	else
	{
		mStatus.deltaX = ( int ) mousePacket[ 1 ];
		mStatus.deltaY = ( int ) mousePacket[ 2 ];

		/* If negative, convert "9-bit signed" value into equivalent
		   "sizeof( int ) signed" value

		   Sign is 9th bit.
		   0x100 is largest negative number in "9-bit signed".

		   https://github.com/SerenityOS/serenity/blob/master/Kernel/Devices/PS2MouseDevice.cpp
		*/
		if ( statusByte & MOUSE_XSIGN )
		{
			mStatus.deltaX -= 0x100;
		}
		if ( statusByte & MOUSE_YSIGN )
		{
			mStatus.deltaY -= 0x100;
		}
	}


	// Save old button status
	mStatus.leftBtnPrev  = mStatus.leftBtn;
	mStatus.rightBtnPrev = mStatus.rightBtn;
	// mStatus.middleBtnPrev = mStatus.middleBtn;


	// Get new button status
	mStatus.leftBtn  = ( statusByte & MOUSE_LEFT )  ? 1 : 0;
	mStatus.rightBtn = ( statusByte & MOUSE_RIGHT ) ? 1 : 0;
	// mStatus.middleBtn = ( statusByte & MOUSE_MIDDLE ) ? 1 : 0;


	/*cprintf(

		"dx: %d, dy: %d, l: %d, r: %d\n",
		mStatus.deltaX, mStatus.deltaY,
		mStatus.leftBtn, mStatus.rightBtn
	);*/

	/*cprintf(

		"dx: %d, dy: %d, l: %d, r: %d, m: %d\n",
		mStatus.deltaX, mStatus.deltaY,
		mStatus.leftBtn, mStatus.rightBtn, mStatus.middleBtn
	);*/



	// Temporary tests ---------------

	vgaHandleMouseEvent();
}


// _____________________________________________________________________________

/* Caller uses this function to get the current mouse status.

   As such current mouse status is retrieved via polling...

   TODO: Is there a way to notify "subscribers" of status change after
         handling 'mouseintr'?


   We let the caller (ex. Window Manager) determine mouse behaviour
   (current location, press, release, drag etc.)

   For example:

        // Handle mouse move event
        if ( _mStatus.deltaX || _mStatus.deltaY )
        {
            onMouseMove();

            // Handle mouse drag event
            if ( _mStatus.leftBtn )
            {
                onMouseDrag();
            }
        }

        //
        if ( _mStatus.leftBtn != _mStatus.leftBtnPrev )
        {
            // Handle mouse press event
            if ( _mStatus.leftBtn )
            {
                onMousePress();
            }

            // Handle mouse release event, mouse click event
            else
            {
                onMouseRelease();

                onMouseClick
            }
        }
*/
void getMouseStatus ( struct mouseStatus* _mStatus )
{
	_mStatus->deltaX        = mStatus.deltaX;
	_mStatus->deltaY        = mStatus.deltaY;
	_mStatus->leftBtn       = mStatus.leftBtn;
	// _mStatus->middleBtn     = mStatus.middleBtn;
	_mStatus->rightBtn      = mStatus.rightBtn;
	_mStatus->leftBtnPrev   = mStatus.leftBtnPrev;
	// _mStatus->middleBtnPrev = mStatus.middleBtnPrev;
	_mStatus->rightBtnPrev  = mStatus.rightBtnPrev;
}
