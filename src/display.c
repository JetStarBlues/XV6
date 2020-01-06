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

static int displayioctl ( struct inode*, int, ... );


void displayinit ( void )
{
	devsw[ DISPLAY ].read  = 0;
	devsw[ DISPLAY ].write = 0;
	devsw[ DISPLAY ].ioctl = displayioctl;
}


// static int displayread ( struct inode* ip, char* dst, int n ) {}
// static int displaywrite ( struct inode* ip, char* src, int n ) {}


static int displayioctl ( struct inode* ip, int request, ... )
{
	if ( request == DISP_IOCTL_SETMODE )
	{
		int sel;

		if ( argint( 2, &sel ) < 0 )
		{
			return - 1;
		}

		vgaSetMode( sel );
	}

	else if ( request == DISP_IOCTL_SETPALCOLOR )
	{
		int colorIdx;
		int r;
		int g;
		int b;

		if ( argint( 2, &colorIdx ) < 0 )
		{
			return - 1;
		}

		if ( argint( 3, &r ) < 0 )
		{
			return - 1;
		}

		if ( argint( 4, &g ) < 0 )
		{
			return - 1;
		}

		if ( argint( 5, &b ) < 0 )
		{
			return - 1;
		}

		vgaSetPaletteColor( colorIdx, ( char ) r, ( char ) g, ( char ) b );
	}

	else if ( request == DISP_IOCTL_DEFAULTPAL )
	{
		vgaSetDefaultPalette();
	}

	#if 0
		else if ( request == DISP_IOCTL_DRAWPIXEL )
		{
			int x;
			int y;
			int colorIdx;

			if ( argint( 2, &x ) < 0 )
			{
				return - 1;
			}

			if ( argint( 3, &y ) < 0 )
			{
				return - 1;
			}

			if ( argint( 4, &colorIdx ) < 0 )
			{
				return - 1;
			}

			vgaWritePixel( x, y, colorIdx );
		}

		else if ( request == DISP_IOCTL_BLIT )
		{
			char* src;

			if ( argptr( 2, &src, sizeof( uchar ) ) < 0 )
			{
				return - 1;
			}

			vgaBlit( ( uchar* ) src );
		}

		else if ( request == DISP_IOCTL_DRAWFILLRECT )
		{
			int x;
			int y;
			int w;
			int h;
			int colorIdx;

			if ( argint( 2, &x ) < 0 )
			{
				return - 1;
			}

			if ( argint( 3, &y ) < 0 )
			{
				return - 1;
			}

			if ( argint( 4, &w ) < 0 )
			{
				return - 1;
			}

			if ( argint( 5, &h ) < 0 )
			{
				return - 1;
			}

			if ( argint( 6, &colorIdx ) < 0 )
			{
				return - 1;
			}

			vgaFillRect( x, y, w, h, colorIdx );
		}

		else if ( request == DISP_IOCTL_DRAWBITMAP8 )
		{
			char* bitmap;
			int   x;
			int   y;
			int   h;
			int   colorIdx;

			if ( argptr( 2, &bitmap, sizeof( uchar ) ) < 0 )
			{
				return - 1;
			}

			if ( argint( 3, &x ) < 0 )
			{
				return - 1;
			}

			if ( argint( 4, &y ) < 0 )
			{
				return - 1;
			}

			if ( argint( 5, &h ) < 0 )
			{
				return - 1;
			}

			if ( argint( 6, &colorIdx ) < 0 )
			{
				return - 1;
			}

			vgaDrawBitmap8( ( uchar* ) bitmap, x, y, h, colorIdx );
		}
	#endif

	else
	{
		cprintf( "displayioctl: unknown request - %d\n", request );

		return - 1;
	}

	return 0;
}
