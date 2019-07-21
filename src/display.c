#include "types.h"
#include "defs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

/*
Based on...
	. https://github.com/sam46/xv6/blob/master/display.c
	. https://github.com/DoctorWkt/xv6-freebsd/blob/master/include/xv6/file.h
*/


// Generic
#define DISP_SETPIXEL   2
#define DISP_BLIT       3
#define DISP_DRAWLINE   9
#define DISP_DRAWRECT   9
#define DISP_DRAWCIRCLE 9

// VGA specific...
#define DISP_SETMODE    1
#define DISP_SETCOLOR   9
#define DISP_DEFAULTPAL 9



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
	if ( request == DISP_SETMODE )
	{
		int sel;

		sel = *argp;

		vgaSetMode( sel );
	}
	else if ( request == DISP_SETPIXEL )
	{
		int x;
		int y;
		int colorIdx;

		x = *argp;
		argp += 1;

		y = *argp;
		argp += 1;

		colorIdx = *argp;
		argp += 1;

		vgaWritePixel( x, y, colorIdx );
	}
	else if ( request == DISP_BLIT )
	{
		uchar* src;

		src = ( uchar* ) *argp;

		vgaBlit( src );
	}
	else
	{
		cprintf( "displayioctl: unknown request - %d\n", request );

		return - 1;
	}

	return 0;
}
