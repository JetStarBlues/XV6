/* Word count */

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

char buf [ 512 ];

void wc ( int fd, char* name )
{
	int i, n;
	int l, w, c, inword;

	l = w = c = 0;
	inword = 0;

	while ( 1 )
	{
		n = read( fd, buf, sizeof( buf ) );

		if ( n <= 0 )
		{
			break;
		}

		for ( i = 0; i < n; i += 1 )
		{
			// Count bytes
			c += 1;

			// Count newlines
			if ( buf[ i ] == '\n' )
			{
				l += 1;
			}

			// Count words
			if ( strchr( " \r\t\n\v", buf[ i ] ) )
			{
				inword = 0;
			}
			else if ( ! inword )
			{
				inword = 1;

				w += 1;
			}
		}
	}

	if ( n < 0 )
	{
		printf( stderr, "wc: read error\n" );

		exit();
	}

	// printf( stdout, "%d %d %d %s\n", l, w, c, name );
	printf( stdout, "file  : %s\n", name );
	printf( stdout, "lines : %d\n", l );
	printf( stdout, "words : %d\n", w );
	printf( stdout, "bytes : %d\n", c );
}

int main ( int argc, char* argv [] )
{
	int fd, i;

	// Use stdin as input
	if ( argc <= 1 )
	{
		wc( stdin, "" );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		if ( ( fd = open( argv[ i ], O_RDONLY ) ) < 0 )
		{
			printf( stderr, "wc: cannot open %s\n", argv[ i ] );

			exit();
		}

		wc( fd, argv[ i ] );

		close( fd );
	}

	exit();
}
