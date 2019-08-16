#include "types.h"

/*
	Mirrors "printf.c"
		. The code in "printf.c" will always be the
		  reference (authoritative) code, and the one here a copy.
		. Duplication only because of kernel vs user space distinction...

	Differences (minor):
		. no fd parameter
			. instead pointer to putc function

		. "printf" is called "kprintf" and has the following parameters:
			. "void ( *putc ) ( int )" - pointer to putc function
			. "char* fmt"              - pointer to string format
			. "uint* argp"             - pointer to variable args

			. Thus:
				. 'putc' is now an argument instead of a static function
				. 'argp' is now an argument instead of a local variable

	The following functions from "ulib.c" and "user.h" also copied:
		. ISDIGIT
		. strlen
		. strchr
		. atoi
*/


// _____________________________________________________________________________

/* Below is copy and paste of needed functions from
   "ulib.c" and "user.h"
*/

// user.h -----------------------------------------------------------

#define ISDIGIT( c ) ( ( ( c ) >= '0' ) && ( ( c ) <= '9' ) )


// ulib.c -----------------------------------------------------------

static uint strlen ( const char* s )
{
	int n;

	for ( n = 0; s[ n ]; n += 1 )
	{
		//
	}

	return n;
}

static char* strchr ( const char* s, char c )
{
	for ( ; *s; s += 1 )
	{
		if ( *s == c )
		{
			return ( char* ) s;
		}
	}

	return 0;
}

static int atoi ( const char* s )
{
	int n;

	n = 0;

	while ( '0' <= *s && *s <= '9' )
	{
		n *= 10;

		n += *s - '0';

		s += 1;
	}

	return n;
}


// _____________________________________________________________________________

/* Below is slightly modified contents of "printf.c".
   See notes at top of page.
*/

#define MAXNDIGITS 16

static char* flags       = "-0";
static char* conversions = "dxpcs";


static void itoa ( int xx, int base, int issigned, char* sign, char sint [] )
{
	static char digits [] = "0123456789ABCDEF";

	char buf [ MAXNDIGITS + 1 ];

	uint x;
	int  i;
	int  j;
	int  isneg;


	// Determine sign
	isneg = 0;

	if ( issigned && ( xx < 0 ) )
	{
		isneg = 1;

		x = - xx;  // abs value
	}
	else
	{
		x = xx;
	}


	// itoa
	i = 0;
	do
	{
		buf[ i ] = digits[ x % base ];

		x /= base;

		i += 1;
	}
	while ( ( x > 0 ) && ( i < MAXNDIGITS ) );


	/* Return integer string.
	   Digits are reversed, so print back to front
	*/
	i -= 1;

	j = 0;

	while ( i >= 0 )
	{
		sint[ j ] = buf[ i ];

		j += 1;

		i -= 1;
	}

	sint[ j ] = 0;  // null terminate


	// Return sign
	if ( issigned )
	{
		if ( isneg )
		{
			*sign = '-';
		}
		else
		{
			*sign = '+';
		}
	}
	else
	{
		*sign = 0;
	}
}

static void printPadding ( void ( *putc ) ( int ), int n, int padWithZero )
{
	while ( n )
	{
		if ( padWithZero )
		{
			putc( '0' );
		}
		else
		{
			putc( ' ' );
		}

		n -= 1;
	}
}

static void print_d ( void ( *putc ) ( int ), uint** argpp, int width, int padLeft, int padRight, int padWithZero )
{
	uint* argp;
	char  sint [ MAXNDIGITS + 1 ];
	char  sign;
	int   i;
	int   x;
	int   npad;
	int   printSign;

	// Get integer from argp
	argp = *argpp;      // create a local copy of argp

	x = ( int ) *argp;  //

	*argpp += 1;        // increment original argp


	// Convert integer to string
	itoa(

		x,
		10,     // base 10
		1,      // signed
		&sign,
		sint
	);


	/* For now, always print sign for negative.
	   In the future, can be parameter (based on flags).
	*/
	printSign = x < 0;


	// Calculate padding
	npad = width - strlen( sint );

	if ( printSign )
	{
		npad -= 1;
	}

	if ( npad < 0 )
	{
		npad = 0;
	}


	/* Print leading characters
	*/
	// If padding with zeros, sign comes first
	if ( padWithZero )
	{
		if ( printSign )
		{
			putc( sign );
		}

		printPadding( putc, npad, 1 );
	}
	// If padding with spaces, padding comes first
	else
	{
		printPadding( putc, npad, 0 );

		if ( printSign )
		{
			putc( sign );
		}
	}

	// Print integer string
	i = 0;

	while ( sint[ i ] )
	{
		putc( sint[ i ] );

		i += 1;
	}

	// Print trailing spaces
	if ( padRight )
	{
		printPadding( putc, npad, 0 );
	}
}

static void print_x ( void ( *putc ) ( int ), uint** argpp, int width, int padLeft, int padRight, int padWithZero )
{
	uint* argp;
	char  sint [ MAXNDIGITS + 1 ];
	char  sign;
	int   i;
	int   x;
	int   npad;

	// Get integer from argp
	argp = *argpp;      // create a local copy of argp

	x = ( int ) *argp;  //

	*argpp += 1;        // increment original argp


	// Convert integer to string
	itoa(

		x,
		16,     // base 16
		0,      // unsigned
		&sign,
		sint
	);


	// Calculate padding
	npad = width - strlen( sint );

	if ( npad < 0 )
	{
		npad = 0;
	}


	// Print leading spaces or zeros
	if ( padLeft )
	{
		printPadding( putc, npad, padWithZero );
	}

	// Print integer string
	i = 0;

	while ( sint[ i ] )
	{
		putc( sint[ i ] );

		i += 1;
	}

	// Print trailing spaces
	if ( padRight )
	{
		printPadding( putc, npad, 0 );
	}
}

static void print_c ( void ( *putc ) ( int ), uint** argpp, int width, int padLeft, int padRight )
{
	uint* argp;
	char  c;
	int   npad;

	// Get char from argp
	argp = *argpp;       // create a local copy of argp

	c = ( char ) *argp;  //

	*argpp += 1;         // increment original argp
	                     /* It seems that chars pushed as int to stack...
	                     */


	// Calculate padding
	npad = width - 1;

	if ( npad < 0 )
	{
		npad = 0;
	}


	// Print leading spaces
	if ( padLeft )
	{
		printPadding( putc, npad, 0 );
	}

	// Print char
	putc( c );

	// Print trailing spaces
	if ( padRight )
	{
		printPadding( putc, npad, 0 );
	}
}

static void print_s ( void ( *putc ) ( int ), uint** argpp, int width, int padLeft, int padRight )
{
	uint* argp;
	char* s;
	int   npad;

	// Get string from argp
	argp = *argpp;        // create a local copy of argp

	s = ( char* ) *argp;  //

	*argpp += 1;          // increment original argp
	                      /* Pointer, sizeof( char* ), seems to be sizeof( int )
	                      */

	// Why so nice?
	if ( s == 0 )
	{
		s = "(null)";
	}


	// Calculate padding
	npad = width - strlen( s );

	if ( npad < 0 )
	{
		npad = 0;
	}


	// Print leading spaces
	if ( padLeft )
	{
		printPadding( putc, npad, 0 );
	}

	// Print string
	while ( *s != 0 )
	{
		putc( *s );

		s += 1;
	}

	// Print trailing spaces
	if ( padRight )
	{
		printPadding( putc, npad, 0 );
	}
}


// _____________________________________________________________________________

/* Tweaked printf function.
   See notes at top of page.
*/

void kprintf ( void ( *putc ) ( int ), const char* fmt, uint* argp )
{
	char  c;
	int   i;

	int flag_padTrailing;
	int flag_padWithZero;

	int  width;
	int  swidthIdx;
	char swidth [ MAXNDIGITS + 1 ];

	// int  precision;
	// int  sprecisionIdx;
	// char sprecision [ MAXNDIGITS + 1 ];

	int padLeft;
	int padRight;


	// Parse format
	for ( i = 0; fmt[ i ]; i += 1 )
	{
		//
		c = fmt[ i ];


		/* The format string contains two types of objects:
		     . ordinary characters
		     . conversion specifications
		   A '%' delineates the latter
		*/

		/* If ordinary character, print it as is
		*/
		if ( c != '%' )
		{
			putc( c );

			continue;
		}		

		/* Otherwise, a conversion has been specified.
		   We will interpret it and print accordingly
		*/

		// Skip the '%'
		i += 1;

		c = fmt[ i ];


		// __ Step 0: Initialize conversion variables ____________

		flag_padTrailing = 0;
		flag_padWithZero = 0;

		width       = 0;
		swidthIdx   = 0;
		swidth[ 0 ] = 0;

		// precision       = 0;
		// sprecisionIdx   = 0;
		// sprecision[ 0 ] = 0;

		padLeft  = 0;
		padRight = 0;


		// __ Step 1: Gather flags _______________________________

		while ( strchr( flags, c ) )
		{
			if ( c == '-' )
			{
				flag_padTrailing = 1;
			}
			else if ( c == '0' )
			{
				flag_padWithZero = 1;
			}

			// Get next character in fmt
			i += 1;

			c = fmt[ i ];
		}


		// __ Step 2: Gather width _______________________________

		if ( ISDIGIT( c ) )
		{
			// Gather number
			while ( ISDIGIT( c ) )
			{
				//
				if ( swidthIdx >= MAXNDIGITS )
				{
					/* Since we can't printf error, we instead
					   silently truncate =(
					*/ 
					break;
				}

				// 
				swidth[ swidthIdx ] = c;

				swidthIdx += 1;


				// Get next character in fmt
				i += 1;

				c = fmt[ i ];
			}

			swidth[ swidthIdx ] = 0;  // null terminate

			width = atoi( swidth );
		}


		// __ Step 3: Gather precision ___________________________

		// TODO


		// __ Step 4: Print formatted value ______________________

		// If valid conversion
		if ( strchr( conversions, c ) )
		{
			// Determine padding
			padLeft  = width && ( ! flag_padTrailing );
			padRight = width &&     flag_padTrailing;


			// Print conversion
			if ( c == 'd' )
			{
				print_d( putc, &argp, width, padLeft, padRight, flag_padWithZero );
			}
			else if ( c == 'x' || c == 'p' )
			{
				print_x( putc, &argp, width, padLeft, padRight, flag_padWithZero );
			}
			else if ( c == 'c' )
			{
				print_c( putc, &argp, width, padLeft, padRight );
			}
			else if ( c == 's' )
			{
				print_s( putc, &argp, width, padLeft, padRight );
			}
		}

		// An escaped '%'
		else if ( c == '%' )
		{
			putc( c );
		}

		// Unknown % sequence. Print it to draw attention.
		else
		{
			putc( '%' );
			putc( c );
		}
	}
}
