#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

char buf[ 512 ];

void cat ( int fd )
{
	int n;

	while ( ( n = read( fd, buf, sizeof( buf ) ) ) > 0 )
	{
		if ( write( 1, buf, n ) != n )
		{
			printf( 2, "cat: write error\n" );

			exit();
		}
	}

	if ( n < 0 )
	{
		printf( 2, "cat: read error\n" );

		exit();
	}
}

int main ( int argc, char* argv [] )
{
	int fd, i;

	// Use stdin as input (ex. from pipe)
	if ( argc <= 1 )
	{
		cat( 0 );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		if ( ( fd = open( argv[ i ], O_RDONLY ) ) < 0 )
		{
			printf( 2, "cat: cannot open %s\n", argv[ i ] );

			exit();
		}

		cat( fd );

		close( fd );
	}

	exit();
}
