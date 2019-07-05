#include "types.h"
#include "defs.h"
// #include "param.h"
// #include "traps.h"
// #include "spinlock.h"
// #include "sleeplock.h"
// #include "fs.h"
// #include "file.h"
// #include "memlayout.h"
// #include "mmu.h"
// #include "proc.h"
#include "x86.h"

#define CRTPORT 0x3d4

static ushort* crt = ( ushort* ) P2V( 0xb8000 );  // CGA memory

static void cgaputc ( int c )
{
	int pos;

	// Cursor position: col + ( 80 * row )
	outb( CRTPORT, 14 );

	pos = inb( CRTPORT + 1 ) << 8;

	outb( CRTPORT, 15 );

	pos |= inb( CRTPORT + 1 );

	if ( c == '\n' )  // newline
	{
		pos += 80 - ( pos % 80 );
	}
	else if ( c == BACKSPACE )
	{
		if ( pos > 0 ) --pos;
	}
	else
	{
		crt[ pos ] = ( c & 0xff ) | 0x0700;  // black on white

		pos += 1;
	}

	if ( pos < 0 || pos > 25 * 80 )
	{
		panic( "cgaputc: pos under/overflow" );
	}

	// Scroll up
	if ( ( pos / 80 ) >= 24 )
	{
		memmove( crt, crt + 80, sizeof( crt[ 0 ] ) * 23 * 80 );

		pos -= 80;

		memset( crt + pos, 0, sizeof( crt[ 0 ] ) * ( 24 * 80 - pos ) );
	}

	outb( CRTPORT, 14 );
	outb( CRTPORT + 1, pos >> 8 );
	outb( CRTPORT, 15 );
	outb( CRTPORT + 1, pos );

	crt[ pos ] = ' ' | 0x0700;
}
