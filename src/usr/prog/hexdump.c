#include "types.h"
#include "user.h"
#include "fcntl.h"

#define INPUTBUFSZ   512
#define BYTESPERLINE 16

/*
Usage:
	hexdump filemane start nbytes

Usage with pipe input:
	hexdump start nbytes

Args:
	start  - offset (in bytes) from start of input.
	         Currently only supports zero

	nbytes - number of bytes to read.
	         If zero, dumps until end of input
*/


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

	// Call to read returned 0, reached EOI
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
	int           isEOI;

	printf( 1, "hexdump: fd %d, start %d, nbytes %d\n", fd, start, nbytes );


	// Check that start is valid
	if ( start < 0 )  // upper bound?
	{
		printf( 2, "hexdump: invalid start (%d)", start );

		return - 1;
	}

	// Check that nbytes is valid
	if ( nbytes < 0 )
	{
		printf( 2, "hexdump: invalid nbytes (%d)", nbytes );

		return - 1;
	}


	/* TODO:
	   for start > 0, lseek? or do we just read (consume input) without printing?
	*/
	if ( start > 0 )
	{
		printf( 2, "hexdump: non-zero start currently unsupported\n" );

		return - 1;
	}


	// Initialize
	addr  = start;
	isEOI = 0;
	i     = 0;
	j     = 0;

	// Zero bpl
	for ( k = 0; k < BYTESPERLINE; k += 1 )
	{
		bpl[ k ] = 0;
	}


	// Print bytes
	while ( ! isEOI )
	{
		// If have read nbytes, done
		if ( nbytes && ( i == nbytes ) )
		{
			break;
		}

		// Get byte
		b = getbyte( fd );

		// If reached end of input, done collecting bytes
		if ( b < 0 )
		{
			isEOI = 1;
		}
		// Otherwise add byte to buffer
		else
		{
			// printf( 1, "addr:%d i:%d j:%d b:%x\n", addr, i, j, b );

			bpl[ j ] = ( unsigned char ) b;

			j += 1;
			i += 1;

			addr += 1;
		}


		// Print a line
		if ( ( j == BYTESPERLINE ) || ( i == nbytes ) || isEOI )
		{
			// Print address
			printf( 1, "%08x: ", addr - j );


			// Print bytes in hexadecimal
			for ( k = 0; k < j; k += 1 )
			{
				// Add space after 8 bytes for legibility
				if ( k == 8 )
				{
					printf( 1, " " );
				}

				// Print byte
				printf( 1, " %02x", bpl[ k ] );
			}
			for ( k = j; k < BYTESPERLINE; k += 1 )
			{
				// Add space after 8 bytes for legibility
				if ( k == 8 )
				{
					printf( 1, " " );
				}

				// Print byte placeholder
				printf( 1, "   " );
			}


			// Print bytes in ASCII
			printf( 1, "  |" );

			for ( k = 0; k < j; k += 1 )
			{
				// Print char if in printable range
				if ( ( bpl[ k ] >= 32 ) && ( bpl[ k ] <= 126 ) )
				{
					printf( 1, "%c", bpl[ k ] );
				}
				// Otherwise print placeholder
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
	printf( 1, "%08x:\n", addr );

	return 0;
}

int main ( int argc, char* argv [] )
{
	int fd;

	// Use stdin as input
	if ( argc == 1 )       // " | hexdump"
	{
		hexdump( 0, 0, 0 );
	}
	else if ( argc == 3 )  // " | hexdump start nbytes"
	{
		hexdump( 0, atoi( argv[ 1 ] ), atoi( argv[ 2 ] ) );
	}

	// Use specified file as input
	else if ( ( argc == 2 ) || ( argc == 4 ) )
	{
		fd = open( argv[ 1 ], O_RDONLY );

		if ( fd < 0 )
		{
			printf( 2, "hexdump: cannot open %s\n", argv[ 1 ] );
		}

		if ( argc == 2 )   // "hexdump filename"
		{
			hexdump( fd, 0, 0 );
		}
		else               // "hexdump filename start nbytes"
		{
			hexdump( fd, atoi( argv[ 2 ] ), atoi( argv[ 3 ] ) );
		}
	}

	//
	else
	{
		printf( 2,

			"Usage:\n"
			"   hexdump filename\n"
			"   hexdump filename start nbytes\n"
		);
	}

	exit();
}
