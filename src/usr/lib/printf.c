#include "types.h"
#include "stat.h"
#include "user.h"

/* Format specification retrieved from:
     The C Programming Language, Kernighan & Ritchie,
     2nd ed. Appendix B 1.2
*/

/* Currently understands:

	. the flags:

		'-' - pad with trailing whitespace.
		      Default is to pad with leading whitespace.

		'0' - pad with leading zeros

	. the conversion characters:

		'd' - print as signed integer with decimal notation
		'x' - print as unsigned integer with hexadecimal notation
		'p' - treated the same as 'x'
		'c' - print as unsigned character with ASCII notation
		's' - print as ASCII string
*/

#define MAXNDIGITS 10

static char* flags       = "-0";
static char* conversions = "dxpcs";


static void putc ( int fd, char c )
{
	write( fd, &c, 1 );
}

static void printint ( int fd, int xx, int base, int sign )
{
	static char digits [] = "0123456789ABCDEF";

	char buf [ 16 ];
	uint x;
	int  i,
	     neg;

	//
	neg = 0;

	if ( sign && xx < 0 )
	{
		neg = 1;

		x = - xx;
	}
	else
	{
		x = xx;
	}


	//
	i = 0;
	do
	{
		buf[ i ] = digits[ x % base ];

		i += 1;
	}
	while ( ( x /= base ) != 0 );


	//
	if ( neg )
	{
		buf[ i ] = '-';

		i += 1;
	}


	//
	i -= 1;

	while ( i >= 0 )
	{
		putc( fd, buf[ i ] );

		i -= 1;
	}
}

// Print to the given fd
// Only understands %d, %x, %p, %c, %s
// Can only print va_arg > 1 of bytes ??
void printf ( int fd, const char* fmt, ... )
{
	uint* argp;
	char  c;
	char* s;
	int   i;

	int flag_padTrailing;
	int flag_padWithZero;

	int         width;
	int         swidthIdx;
	static char swidth [ MAXNDIGITS + 1 ];

	// int         precision;
	// int         sprecisionIdx;
	// static char sprecision [ MAXNDIGITS + 1 ];


	// Create pointer to variable args...
	argp = ( ( uint* ) ( void* ) &fmt ) + 1;


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
			putc( fd, c );

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
			// Print leading spaces
			while ( width && ( ! flag_padTrailing ) )
			{
				if ( flag_padWithZero )
				{
					putc( fd, '0' );
				}
				else
				{
					putc( fd, ' ' );
				}

				width -= 1;
			}


			// Print conversion
			if ( c == 'd' )
			{
				printint( fd, *argp, 10, 1 );

				argp += 1;
			}
			else if ( c == 'x' || c == 'p' )
			{
				printint( fd, *argp, 16, 0 );

				argp += 1;
			}
			else if ( c == 'c' )
			{
				putc( fd, ( char ) *argp );

				argp += 1;  // it seems that chars pushed as int to stack...
			}
			else if ( c == 's' )
			{
				s = ( char* ) *argp;

				argp += 1;  // pointer, sizeof( char* ), seems to be sizeof( int )

				if ( s == 0 )
				{
					s = "(null)";
				}

				while ( *s != 0 )
				{
					putc( fd, *s );

					s += 1;
				}
			}


			// Print trailing spaces
			while ( width && flag_padTrailing )
			{
				putc( fd, ' ' );

				width -= 1;
			}
		}


		// An escaped '%'
		else if ( c == '%' )
		{
			putc( fd, c );
		}


		// Unknown % sequence. Print it to draw attention.
		else
		{
			putc( fd, '%' );
			putc( fd, c );
		}
	}
}
