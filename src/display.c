#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "display.h"

/*
Based on:
	. https://github.com/sam46/xv6/blob/master/display.c
	. https://github.com/DoctorWkt/xv6-freebsd/blob/master/include/xv6/file.h
*/

static int displayioctl ( struct inode*, int, uint* );


void displayinit ( void )
{
	devsw[ DISPLAY ].read  = 0;
	devsw[ DISPLAY ].write = 0;
	devsw[ DISPLAY ].ioctl = displayioctl;
}


// int displayread ( struct inode* ip, char* dst, int n ) {}
// int displaywrite ( struct inode* ip, char* src, int n ) {}


static int displayioctl ( struct inode* ip, int request, uint* argp )
{
	if ( request == DISP_IOCTL_SETMODE )
	{
		uint sel;

		sel = *argp;

		vgaSetMode( sel );
	}

	else if ( request == DISP_IOCTL_SETPALCOLOR )
	{
		int  index;
		char r;
		char g;
		char b;

		index = ( int ) *argp;
		argp += 1;

		// We assume r,g,b passed as ints
		r = ( char ) *argp;
		argp += 1;

		g = ( char ) *argp;
		argp += 1;

		b = ( char ) *argp;
		argp += 1;


		vgaSetPaletteColor( index, r, g, b );
	}

	else if ( request == DISP_IOCTL_DEFAULTPAL )
	{
		vgaSetDefaultPalette();
	}

	else if ( request == DISP_IOCTL_DRAWPIXEL )
	{
		int x;
		int y;
		int colorIdx;

		x = ( int ) *argp;
		argp += 1;

		y = ( int ) *argp;
		argp += 1;

		colorIdx = ( int ) *argp;
		argp += 1;

		vgaWritePixel( x, y, colorIdx );
	}

	else if ( request == DISP_IOCTL_BLIT )
	{
		uchar* src;

		src = ( uchar* ) *argp;

		vgaBlit( src );
	}

	else if ( request == DISP_IOCTL_DRAWFILLRECT )
	{
		int x;
		int y;
		int w;
		int h;
		int colorIdx;

		x = ( int ) *argp;
		argp += 1;

		y = ( int ) *argp;
		argp += 1;

		w = ( int ) *argp;
		argp += 1;

		h = ( int ) *argp;
		argp += 1;

		colorIdx = ( int ) *argp;
		argp += 1;

		vgaFillRect( x, y, w, h, colorIdx );
	}

	else
	{
		cprintf( "displayioctl: unknown request - %d\n", request );

		return - 1;
	}

	return 0;
}
