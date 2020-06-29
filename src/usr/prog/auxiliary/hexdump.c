#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

#define INPUTBUFSZ   512
#define BYTESPERLINE 16

/*
Usage:
	hexdump filemane start nbytes

Usage with pipe input:
	hexdump start nbytes

Args:
	start  - offset (in bytes) from start of input.

	nbytes - number of bytes to read.
	         If zero, dumps until end of input
*/


/* getbyte based on:
     The C Programming Language, Kernighan & Ritchie,
     2nd ed. Section 8.2
*/
/* Returns:
     byte on successful read
     (-1) on EOF
     (-2) on error
*/
static int getbyte ( int fd )
{
	static char  buf [ INPUTBUFSZ ];
	static char* bufp;
	static int   n = 0;

	int ret;

	// Buffer is empty, fill it
	if ( n == 0 )
	{
		// Read bytes from fd
		n = read( fd, buf, INPUTBUFSZ );

		// Reached EOI, alert caller
		if ( n == 0 )
		{
			return - 1;  // (int) -1 falls outside char range...
		}
		// Error
		else if ( n < 0 )
		{
			// printf( 2, "getbyte: read error\n" );

			return - 2;
		}

		// printf( 1, "getbyte: read %d bytes\n", n );

		bufp = buf;
	}


	// Read from buffer
	ret = ( int ) ( ( unsigned char ) *bufp );

	bufp += 1;

	n -= 1;

	return ret;
}

/* TODO:
     For pipe, no choice but to read and discard (I think).
     However for file, it is probably worthwhile to use lseek
*/
static int skipbytes ( int fd, int nbytes )
{
	char buf [ INPUTBUFSZ ];

	int n;
	int nToRead;
	int nSkipped;

	// TODO, check if fd is file and use lseek
	/*if ( fstat( fd, &st ) )
	{
		if ( st.type == T_FILE )
		{
			lseek

			return;
		}
	}*/

	// Read all the bytes at once
	if ( nbytes <= INPUTBUFSZ )
	{
		n = read( fd, buf, nbytes );

		if ( n < 0 )
		{
			printf( 2, "hexdump: skipbytes: read error\n" );

			exit();
		}

		// printf( 1, "hexdump: skipbytes: skipped %d bytes\n", n );

		nSkipped = n;
	}
	// Read chunks at a time
	else
	{
		nToRead  = nbytes;
		nSkipped = 0;

		while ( nToRead > 0 )
		{
			if ( nToRead <= INPUTBUFSZ )
			{
				n = read( fd, buf, nToRead );

				if ( n < 0 )
				{
					printf( 2, "hexdump: skipbytes: read error\n" );

					exit();
				}

				// printf( 1, "hexdump: skipbytes: skipped %d bytes\n", n );

				nToRead  -= n;
				nSkipped += n;
			}
			else
			{
				n = read( fd, buf, INPUTBUFSZ );

				if ( n < 0 )
				{
					printf( 2, "hexdump: skipbytes: read error\n" );

					exit();
				}

				// printf( 1, "hexdump: skipbytes: skipped %d bytes\n", n );

				nToRead  -= n;
				nSkipped += n;
			}
		}
	}

	return nSkipped;
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


	// Skip to start...
	if ( start > 0 )
	{
		/* Number of bytes skipped might be less
		   than requested (ex. EOI reached), so we
		   set start to actual number skipped...
		*/
		start = skipbytes( fd, start );
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

		if ( b < - 1 )
		{
			printf( 2, "getbyte error\n" );

			exit();
		}


		// If reached end of input, done collecting bytes
		if ( b == - 1 )
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
		/* TODO: Some way to detect whether called with
		    no args because pipe or because typed in
		    terminal.
		    If typed in terminal, want to instead
		    show usage message...
		*/

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
