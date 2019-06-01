#include "types.h"
#include "stat.h"
#include "user.h"

int main ( int argc, char **argv )
{
	int i;

	if ( argc < 2 )
	{
		printf( 2, "usage: kill pid...\n" );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		kill( atoi( argv[ i ] ) );
	}

	exit();
}
