// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

/* Why is panic detected and handled here ??
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"

#define BACKSPACE 0x100
#define INPUT_BUF 128

#define C( x ) ( ( x ) - '@' )  // Control-x


static struct
{
	/* This lock is used for ... ??
	*/
	struct spinlock lock;
	int             locking;

} cons;

struct {

	char buf [ INPUT_BUF ];
	uint r;                  // Read index
	uint w;                  // Write index
	uint e;                  // Edit index

} input;

static int panicked = 0;

static void consputc     ( int );
int         consoleread  ( struct inode*, char*, int );
int         consolewrite ( struct inode*, char*, int );


void consoleinit ( void )
{
	initlock( &cons.lock, "console" );

	devsw[ CONSOLE ].write = consolewrite;
	devsw[ CONSOLE ].read  = consoleread;

	cons.locking = 1;  // Why distinguish ??

	ioapicenable( IRQ_KBD, 0 );
}


// _____________________________________________________________________________

static void printint ( int xx, int base, int sign )
{
	static char digits [] = "0123456789abcdef";
	char        buf [ 16 ];
	int         i;
	uint        x;

	if ( sign && ( sign = xx < 0 ) )
	{
		x = - xx;
	}
	else
	{
		x = xx;
	}

	i = 0;
	do
	{
		buf[ i ] = digits[ x % base ];

		i += 1;
	}
	while ( ( x /= base ) != 0 );

	if ( sign )
	{
		buf[ i ] = '-';

		i += 1;
	}

	while ( --i >= 0 )
	{
		consputc( buf[ i ] );
	}
}

// Print to the console
// Only understands %d, %x, %p, %c, %s,
void cprintf ( char* fmt, ... )
{
	int   i,
	      c,
	      locking;
	uint* argp;
	char* s;

	locking = cons.locking;

	if ( locking )
	{
		acquire( &cons.lock );
	}

	if ( fmt == 0 )
	{
		panic( "cprintf: null fmt" );
	}

	argp = ( uint* ) ( void* ) ( &fmt + 1 );

	for ( i = 0; ( c = fmt[ i ] & 0xff ) != 0; i += 1 )
	{
		if ( c != '%' )
		{
			consputc( c );

			continue;
		}

		i += 1;

		c = fmt[ i ] & 0xff;

		if ( c == 0 )
		{
			break;
		}

		switch ( c )
		{
			case 'd':

				printint( *argp, 10, 1 );

				argp += 1;

				break;

			case 'x':
			case 'p':

				printint( *argp, 16, 0 );

				argp += 1;

				break;

			case 'c':

				consputc( *argp );

				argp += 1;

				break;

			case 's':

				s = ( char* ) *argp;

				argp += 1;

				if ( s == 0 )
				{
					s = "(null)";
				}

				for ( ; *s; s += 1 )
				{
					consputc( *s );
				}

				break;

			case '%':

				consputc( '%' );

				break;

			default:

				// Print unknown % sequence to draw attention.
				consputc( '%' );
				consputc( c );

				break;
		}
	}

	if ( locking )
	{
		release( &cons.lock );
	}
}


// _____________________________________________________________________________

void panic ( char* s )
{
	int  i;
	uint pcs [ 10 ];

	// Disable interrupts
	cli();

	cons.locking = 0;

	// use lapiccpunum so that we can call panic from mycpu()
	cprintf( "\n" );
	cprintf( "Panic!\n" );
	cprintf( "msg       : %s\n", s );
	cprintf( "lapicid   : %d\n", lapicid() );
	cprintf( "callstack :\n\n" );

	getcallerpcs( &s, pcs );

	for ( i = 0; i < 10; i += 1 )
	{
		cprintf( "    %p\n", pcs[ i ] );
	}

	panicked = 1;  // freeze other CPUs

	// Freeze this CPU
	for ( ;; )
	{
		//
	}
}


// _____________________________________________________________________________

void consputc ( int c )
{
	// If panicked, freeze this CPU
	if ( panicked )
	{
		cli();  // disable interrupts

		for ( ;; )
		{
			//
		}
	}


	if ( c == BACKSPACE )
	{
		uartputc( '\b' );
		uartputc( ' ' );  // overwrite character on display with space
		uartputc( '\b' );
	}
	else
	{
		uartputc( c );
	}

	vgaputc( c );
}


// _____________________________________________________________________________

void consoleintr ( int ( *getc ) ( void ) )
{
	int c,
	    doprocdump = 0;

	acquire( &cons.lock );

	while ( ( c = getc() ) >= 0 )
	{
		switch ( c )
		{
			// Process listing
			case C( 'P' ):

				// procdump() locks cons.lock indirectly; invoke later
				doprocdump = 1;

				break;

			// Kill line
			case C( 'U' ):

				while ( input.e != input.w &&
				        input.buf[ ( input.e - 1 ) % INPUT_BUF] != '\n' )
				{
					input.e -= 1;

					consputc( BACKSPACE );
				}

				break;

			// Backspace
			case C( 'H' ):
			case '\x7f':

				if ( input.e != input.w )
				{
					input.e -= 1;

					consputc( BACKSPACE );
				}

				break;

			default:

				if ( c != 0 && input.e - input.r < INPUT_BUF )
				{
					c = ( c == '\r' ) ? '\n' : c;

					input.buf[ input.e % INPUT_BUF ] = c;

					input.e += 1;

					consputc( c );

					// Gather until either of the following conditions met,
					// then wakeup whoever is sleeping on input
					if ( c == '\n'                        ||  // enter key
					     c == C( 'D' )                    ||  // ctrl-D
					     input.e == input.r + INPUT_BUF )     // input buffer is full
					{
						input.w = input.e;

						wakeup( &input.r );
					}
				}

				break;
		}
	}

	release( &cons.lock );

	if ( doprocdump )
	{
		procdump();  // now call procdump() wo. cons.lock held
	}
}


// _____________________________________________________________________________

int consoleread ( struct inode* ip, char* dst, int n )
{
	uint target;
	int  c;

	iunlock( ip );

	target = n;

	acquire( &cons.lock );

	while ( n > 0 )
	{
		while ( input.r == input.w )
		{
			if ( myproc()->killed )
			{
				release( &cons.lock );

				ilock( ip );

				return - 1;
			}

			sleep( &input.r, &cons.lock );
		}

		c = input.buf[ input.r % INPUT_BUF ];

		input.r += 1;

		if ( c == C( 'D' ) )  // EOF
		{
			if ( n < target )
			{
				// Save ^D for next time, to make sure
				// caller gets a 0-byte result.
				input.r -= 1;
			}

			break;
		}

		*dst = c;

		dst += 1;

		n -= 1;

		if ( c == '\n' )
		{
			break;
		}
	}

	release( &cons.lock );

	ilock( ip );

	return target - n;
}

int consolewrite ( struct inode* ip, char* buf, int n )
{
	int i;

	iunlock( ip );

	acquire( &cons.lock );

	for ( i = 0; i < n; i += 1 )
	{
		consputc( buf[ i ] & 0xff );
	}

	release( &cons.lock );

	ilock( ip );

	return n;
}
