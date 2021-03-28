#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	if ( argc != 3 )
	{
		printf( stderr, "Usage: ln old new\n" );

		exit();
	}

	if ( link( argv[ 1 ], argv[ 2 ] ) < 0 )
	{
		printf( stderr, "ln: link %s %s failed\n", argv[ 1 ], argv[ 2 ] );
	}

	exit();
}
