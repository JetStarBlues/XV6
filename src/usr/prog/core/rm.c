#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int i;

	if ( argc < 2 )
	{
		printf( stderr, "Usage: rm files...\n" );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		if ( unlink( argv[ i ] ) < 0 )
		{
			printf( stderr, "rm: %s failed to delete\n", argv[ i ] );

			break;
		}
	}

	exit();
}
