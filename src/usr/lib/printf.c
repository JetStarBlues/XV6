#include "types.h"
#include "stat.h"
#include "user.h"

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

	neg = 0;

	if ( sign && xx < 0 )
	{
		neg = 1;

		x = -xx;
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

	if ( neg )
	{
		buf[ i ] = '-';

		i += 1;
	}

	while ( --i >= 0 )
	{
		putc( fd, buf[ i ] );
	}
}

// Print to the given fd. Only understands %d, %x, %p, %s, %c.
void printf ( int fd, const char *fmt, ... )
{
	char *s;
	int   c,
	      i,
	      state;
	uint *argp;

	state = 0;

	argp = ( uint* )( void* )&fmt + 1;

	for ( i = 0; fmt[ i ]; i += 1 )
	{
		c = fmt[ i ] & 0xff;

		if ( state == 0 )
		{
			if ( c == '%' )
			{
				state = '%';
			}
			else
			{
				putc( fd, c );
			}
		}
		else if ( state == '%' )
		{
			if ( c == 'd' )
			{
				printint( fd, *argp, 10, 1 );

				argp += 1;  // is this plus one byte? If so can't chain integers of datatype > char
			}
			else if ( c == 'x' || c == 'p' )
			{
				printint( fd, *argp, 16, 0 );

				argp += 1;  // is this plus one byte? If so can't chain integers of datatype > char
			}
			else if ( c == 's' )
			{
				s = ( char* )*argp;

				argp += 1;

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
			else if ( c == 'c' )
			{
				putc( fd, *argp );

				argp += 1;
			}
			else if ( c == '%' )
			{
				putc( fd, c );
			}
			else
			{
				// Unknown % sequence.  Print it to draw attention.
				putc( fd, '%' );
				putc( fd, c );
			}

			state = 0;
		}
	}
}
