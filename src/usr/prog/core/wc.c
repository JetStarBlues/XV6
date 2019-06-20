/*** Word count ***/
#include "types.h"
#include "stat.h"
#include "user.h"

char buf [ 512 ];

void wc ( int fd, char *name )
{
	int i, n;
	int l, w, c, inword;

	l = w = c = 0;
	inword = 0;

	while ( ( n = read( fd, buf, sizeof( buf ) ) ) > 0 )
	{
		for ( i = 0; i < n; i += 1 )
		{
			c += 1;

			if ( buf[ i ] == '\n' )
			{
				l += 1;
			}

			if ( strchr( " \r\t\n\v", buf[i] ) )
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
		printf( 1, "wc: read error\n" );

		exit();
	}

	// printf( 1, "%d %d %d %s\n", l, w, c, name );
	printf( 1, "file  : %s\n", name );
	printf( 1, "lines : %d\n", l );
	printf( 1, "words : %d\n", w );
	printf( 1, "bytes : %d\n", c );
}

int main ( int argc, char *argv[] )
{
	int fd, i;

	if ( argc <= 1 )  // input from pipe
	{
		wc( 0, "" );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		if ( ( fd = open( argv[ i ], 0 ) ) < 0 )
		{
			printf( 1, "wc: cannot open %s\n", argv[ i ] );

			exit();
		}

		wc( fd, argv[ i ] );

		close( fd );
	}

	exit();
}