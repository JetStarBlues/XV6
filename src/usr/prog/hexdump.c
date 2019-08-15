#include "types.h"
#include "user.h"
#include "fcntl.h"


#define INPUTBUFSZ   512
#define BYTESPERLINE 16


/* getbyte code from:
     The C Programming Language, Kernighan & Ritchie,
     2nd ed. Section 8.2
*/
int getbyte ( int fd )
{
	static char  buf [ INPUTBUFSZ ];
	static char* bufp;
	static int   n = 0;

	int ret;

	// Buffer is empty
	if ( n == 0 )
	{
		n = read( fd, buf, sizeof( buf ) );

		// printf( 1, "hexdump: getbyte: read %d bytes\n", n );

		bufp = buf;
	}

	// Read from buffer
	n -= 1;

	if ( n >= 0 )
	{
		ret = ( int ) ( ( unsigned char ) *bufp );

		bufp += 1;

		return ret;
	}

	// Call to read returned 0, reached EOF
	else if ( n == - 1 )
	{
		return - 1;  // (int) -1 falls outside char range...
	}
	// Call to read returned a negative value
	else
	{
		printf( 2, "hexdump: read error\n" );

		exit();
	}
}

int hexdump ( int fd, int start, int nbytes )
{
	int           b;
	unsigned char bpl [ BYTESPERLINE ];
	int           i, j, k;
	int           addr;
	int           isEOF;

	// TODO - if nbytes 0, read till EOF

	// TODO - check start and nbytes are valid...
	if ( start < 0 )
	{
		printf( 2, "hexdump: invalid start (%d)", start );

		return - 1;
	}
	if ( nbytes < 0 )
	{
		printf( 2, "hexdump: invalid nbytes (%d)", nbytes );

		return - 1;
	}

	// TODO - for start > 0, need lseek =/
	if ( start > 0 )
	{
		printf( 2, "hexdump: non-zero start currently unsupported\n" );

		return - 1;
	}


	// addr = ( char* ) start;

	addr  = start;
	isEOF = 0;
	i     = 0;
	j     = 0;

	// Zero bpl
	for ( k = 0; k < BYTESPERLINE; k += 1 )
	{
		bpl[ k ] = 0;
	}

	while ( ( i < nbytes ) && ( ! isEOF ) )
	{
		// Get byte
		b = getbyte( fd );

		if ( b < 0 )
		{
			isEOF = 1;
		}
		else
		{
			bpl[ j ] = ( unsigned char ) b;

			// printf( 1, "addr:%d i:%d j:%d b:%x\n", addr, i, j, b );

			j += 1;
			i += 1;

			addr += 1;
		}


		// Print a line
		if ( ( j == BYTESPERLINE ) || ( i == nbytes ) || isEOF )
		{
			// Print address
			printf( 1, "%x:.", addr - j );


			// Print bytes in hexadecimal
			for ( k = 0; k < j; k += 1 )
			{
				// Add space after 8 bytes for legibility
				if ( k == 8 )
				{
					printf( 1, "$" );
				}

				printf( 1, ".%x", bpl[ k ] );
			}
			for ( k = j; k < BYTESPERLINE; k += 1 )
			{
				// Add space after 8 bytes for legibility
				if ( k == 8 )
				{
					printf( 1, "$" );
				}

				printf( 1, ".&" );
			}


			// Print bytes in ASCII
			printf( 1, "..|" );

			for ( k = 0; k < j; k += 1 )
			{
				if ( ( bpl[ k ] >= 32 ) && ( bpl[ k ] <= 126 ) )  // printable range
				{
					printf( 1, "%c", bpl[ k ] );
				}
				else
				{
					printf( 1, "." );
				}
			}

			printf( 1, "|\n" );


			// Zero bpl
			for ( k = 0; k < BYTESPERLINE; k += 1 )
			{
				bpl[ k ] = 0;
			}

			j = 0;
		}
	}

	// Print last address
	printf( 1, "%x:\n", addr );

	return 0;
}

int main ( int argc, char* argv [] )
{
	int fd;

	// Use stdin as input (ex. from pipe)
	if ( argc != 4 )
	{
		printf( 2, "Usage: hexdump filename start nbytes\n" );

		exit();
	}

	fd = open( argv[ 1 ], O_RDONLY );

	if ( fd < 0 )
	{
		printf( 2, "hexdump: cannot open %s\n", argv[ 1 ] );
	}

	hexdump( fd, atoi( argv[ 2 ] ), atoi( argv[ 3 ] ) );

	exit();
}
