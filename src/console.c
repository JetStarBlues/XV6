// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

/* Why is panic detected and handled here ??
*/

/* Seems specifying between 0,1,2 for stdin/out/err is
   irrelevant since by the time consolewrite is called,
   fd information has been lost
*/

/* TODO:
   - understand how input buffer works
   - stop CTRL+D from printing a character...
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

#define BACKSPACE  0x100
#define INPUTBUFSZ 128

#define C( x ) ( ( x ) - '@' )  // Control-x


struct _cons {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;
	int             locking;

};

static struct _cons   cons;

struct _input {

	char buf [ INPUTBUFSZ ];
	uint rdIdx;  // Read index
	uint wrIdx;  // Write index
	uint edIdx;  // Edit index

};

static struct _input input;

static int panicked = 0;


void       cprintf      ( char*, ... );
static int consoleread  ( struct inode*, char*, int );
static int consolewrite ( struct inode*, char*, int );


void consoleinit ( void )
{
	initlock( &cons.lock, "console" );

	devsw[ CONSOLE ].read  = consoleread;
	devsw[ CONSOLE ].write = consolewrite;
	devsw[ CONSOLE ].ioctl = 0;

	cons.locking = 1;  // Why distinguish ??

	ioapicenable( IRQ_KBD, 0 );
}


// _____________________________________________________________________________

#define CONSNPCS 10

void panic ( char* s )
{
	int  i;
	uint pcs [ CONSNPCS ];

	// Disable interrupts
	cli();

	cons.locking = 0;

	// use lapiccpunum so that we can call panic from mycpu()
	cprintf( "\n" );
	cprintf( "Panic!\n" );
	cprintf( "msg       : %s\n", s );
	cprintf( "lapicid   : %d\n", lapicid() );
	cprintf( "callstack :\n" );

	getcallerpcs( &s, pcs, CONSNPCS );

	for ( i = 0; i < CONSNPCS; i += 1 )
	{
		if ( pcs[ i ] == 0 )
		{
			break;
		}

		cprintf( "    %p\n", pcs[ i ] );
	}

	cprintf( "\n" );

	panicked = 1;  // freeze other CPUs

	// Freeze this CPU
	for ( ;; )
	{
		//
	}
}


// _____________________________________________________________________________

static void consputc ( int c )
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
		uartputc( ' '  );  // overwrite character on display with space
		uartputc( '\b' );
	}
	else
	{
		uartputc( c );
	}

	vgaputc( c );  // code in vgaputc handles BACKSPACE
}


// _____________________________________________________________________________

void consoleintr ( int ( *getc ) ( void ) )
{
	int c;
	int doprocdump = 0;
	int dotestthing = 0;

	acquire( &cons.lock );

	while ( 1 )
	{
		c = getc();

		if ( c < 0 )
		{
			break;
		}

		switch ( c )
		{
			// DELETE ME! Temporary for testing
			case C( 'T' ):

				dotestthing = 1;

				break;

			// Process listing
			case C( 'P' ):

				// procdump() locks cons.lock indirectly; invoke later
				doprocdump = 1;

				break;

			// Kill line
			case C( 'U' ):

				while ( input.edIdx != input.wrIdx  &&                          // Haven't reached ??
				        input.buf[ ( input.edIdx - 1 ) % INPUTBUFSZ] != '\n' )  // Haven't reached end of previous line
				{
					input.edIdx -= 1;

					consputc( BACKSPACE );
				}

				break;

			// Backspace
			case C( 'H' ):
			case '\x7f':

				if ( input.edIdx != input.wrIdx )  // ??
				{
					input.edIdx -= 1;

					consputc( BACKSPACE );
				}

				break;

			default:

				if ( ( c != 0 ) &&
					 ( input.edIdx - input.rdIdx < INPUTBUFSZ ) )  // ??
				{
					c = ( c == '\r' ) ? '\n' : c;

					input.buf[ input.edIdx % INPUTBUFSZ ] = c;

					input.edIdx += 1;

					consputc( c );

					// Gather until either of the following conditions met,
					// then wakeup whoever is sleeping on input
					if ( c == '\n'                                ||  // enter key
					     c == C( 'D' )                            ||  // ctrl-D
					     input.edIdx == input.rdIdx + INPUTBUFSZ )    // ?? input buffer is full
					{
						input.wrIdx = input.edIdx;

						wakeup( &input.rdIdx );
					}
				}

				break;
		}
	}

	release( &cons.lock );

	if ( doprocdump )
	{
		procdump();  // now call procdump() without cons.lock held
	}

	if ( dotestthing )
	{
		cprintf( "Hacker thingies!\n" );

		demoGraphics();

		// vgaSetMode( 0x03 );  // text mode

		// cprintf( "Did we switch back to text mode successfully?\n" );
	}
}


// _____________________________________________________________________________

static int consoleread ( struct inode* ip, char* dst, int n )
{
	uint target;
	int  c;

	iunlock( ip );

	target = n;

	acquire( &cons.lock );

	/* Read input.buf until either:
	     . n bytes have been read
	     . encounter 'ctrl-D'
	     . encounter '\n'
	*/
	while ( n > 0 )
	{
		while ( input.rdIdx == input.wrIdx )
		{
			if ( myproc()->killed )
			{
				release( &cons.lock );

				ilock( ip );

				return - 1;
			}

			sleep( &input.rdIdx, &cons.lock );
		}

		c = input.buf[ input.rdIdx % INPUTBUFSZ ];

		input.rdIdx += 1;

		if ( c == C( 'D' ) )  // EOF
		{
			if ( n < target )
			{
				/* Save Ctrl+D for next time, to make sure
				   caller gets a 0-byte result.
				*/
				input.rdIdx -= 1;
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

static int consolewrite ( struct inode* ip, char* buf, int n )
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


// _____________________________________________________________________________

// Print to the console
void cprintf ( char* fmt, ... )
{
	int   locking;
	uint* argp;

	locking = cons.locking;

	if ( locking )
	{
		acquire( &cons.lock );
	}

	if ( fmt == 0 )
	{
		panic( "cprintf: null fmt" );
	}

	// Create pointer to variable args...
	argp = ( ( uint* ) ( void* ) &fmt ) + 1;


	// Call kprintf to do the actual printing
	kprintf( &consputc, fmt, argp );


	if ( locking )
	{
		release( &cons.lock );
	}
}
