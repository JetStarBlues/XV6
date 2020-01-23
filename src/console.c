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
#include "date.h"
#include "fs.h"
#include "file.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "termios.h"

#define BACKSPACE  0x100
#define INPUTBUFSZ 128

#define C( x ) ( ( x ) - '@' )  // Control-x


struct _cons {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;
	int             locking;

	// Used to configure console behaviour
	struct termios termios;
};

static struct _cons cons;

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
static int consoleioctl ( struct inode*, int, ... );


void consoleinit ( void )
{
	initlock( &cons.lock, "console" );

	devsw[ CONSOLE ].read  = consoleread;
	devsw[ CONSOLE ].write = consolewrite;
	devsw[ CONSOLE ].ioctl = consoleioctl;

	cons.locking = 1;  // ??

	// default as canonical-mode input with echo
	cons.termios.echo   = 1;
	cons.termios.icanon = 1;

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

static void console_echo ( int c )
{
	if ( cons.termios.echo )
	{
		consputc( c );
	}
}


// _____________________________________________________________________________

void consoleintr ( int ( *getc ) ( void ) )
{
	int c;
	int doprocdump = 0;
	int dotestthing = 0;

	//
	acquire( &cons.lock );


	//
	while ( 1 )
	{
		//
		c = getc();

		if ( c < 0 )
		{
			break;
		}


		// Canonical-mode input
		if ( cons.termios.icanon )
		{
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

						console_echo( BACKSPACE );
					}

					break;

				// Backspace
				case C( 'H' ):
				case '\x7f':

					if ( input.edIdx != input.wrIdx )  // ??
					{
						input.edIdx -= 1;

						console_echo( BACKSPACE );
					}

					break;

				default:

					if ( ( c != 0 ) &&
						 ( input.edIdx - input.rdIdx < INPUTBUFSZ ) )  // ??
					{
						// Read carriage return '\r' as newline '\n'
						c = ( c == '\r' ) ? '\n' : c;


						//
						input.buf[ input.edIdx % INPUTBUFSZ ] = c;

						input.edIdx += 1;


						//
						console_echo( c );


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


		// Raw-mode input
		else
		{
			if ( ( c != 0 ) &&
				 ( input.edIdx - input.rdIdx < INPUTBUFSZ ) )  // ??
			{
				// Read carriage return '\r' as newline '\n'
				/* Pure raw-mode would return the '\r' as is...
				*/
				c = ( c == '\r' ) ? '\n' : c;


				//
				input.buf[ input.edIdx % INPUTBUFSZ ] = c;

				input.edIdx += 1;


				//
				console_echo( c );


				// Wakeup whoever is sleeping on input
				input.wrIdx = input.edIdx;

				wakeup( &input.rdIdx );
			}
		}
	}


	//
	release( &cons.lock );


	//
	if ( doprocdump )
	{
		procdump();  // now call procdump() without cons.lock held
	}

	if ( dotestthing )
	{
		cprintf( "Hacker thingies!\n" );

		// demoGraphics();

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

		// If receive EOF, while in canonical-mode input...
		if ( ( c == C( 'D' ) ) && cons.termios.icanon )
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

		// If receive newline, while in canonical-mode input...
		if ( ( c == '\n' ) && cons.termios.icanon )
		{
			break;
		}
	}

	release( &cons.lock );

	ilock( ip );

	return target - n;
}


// _____________________________________________________________________________

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

static int _getAttribute ( struct termios* termios_p )
{
	/* Copy cons.termios to *termios_p */
	// *termios_p = cons.termios;
	memcpy( termios_p, &( cons.termios ), sizeof( struct termios ) );

	return 0;
}

static int _setAttribute ( struct termios* termios_p )
{
	/* Copy *termios_p to cons.termios */
	// cons.termios = *termios_p;
	memcpy( &( cons.termios ), termios_p, sizeof( struct termios ) );

	return 0;
}

static int consoleioctl ( struct inode* ip, int request, ... )
{
	if ( request == CONS_IOCTL_GETATTR )
	{
		char* termios_p;

		if ( argptr( 2, &termios_p, sizeof( struct termios ) ) < 0 )
		{
			return - 1;
		}

		return _getAttribute( ( struct termios* ) termios_p );

	}
	else if ( request == CONS_IOCTL_SETATTR )
	{
		char* termios_p;

		if ( argptr( 2, &termios_p, sizeof( struct termios ) ) < 0 )
		{
			return - 1;
		}

		return _setAttribute( ( struct termios* ) termios_p );
	}
	else
	{
		cprintf( "consoleioctl: unknown request - %d\n", request );

		return - 1;
	}

	return 0;
}


// _____________________________________________________________________________

// Print to the console
void cprintf ( char* fmt, ... )
{
	int     locking;
	va_list argp;


	//
	locking = cons.locking;

	if ( locking )
	{
		acquire( &cons.lock );
	}


	// Call kprintf to do the actual printing
	va_start( argp, fmt );

	vkprintf( &consputc, fmt, argp );

	va_end( argp );


	//
	if ( locking )
	{
		release( &cons.lock );
	}
}
