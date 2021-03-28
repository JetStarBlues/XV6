#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int i;

	if ( argc < 2 )
	{
		printf( stderr, "Usage: mkdir files...\n" );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		if ( mkdir( argv[ i ] ) < 0 )
		{
			printf( stderr, "mkdir: %s failed to create\n", argv[ i ] );

			break;
		}
	}

	exit();
}
