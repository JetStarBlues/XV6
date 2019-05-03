// https://youtu.be/YPIw67LuuY4

#include "types.h"
#include "user.h"

int main ( int argc, char *argv[] )
{
	int i,
	    x,
	    n;

	x = 0;
	n = atoi( argv[ 1 ] );  // get from command line

	for ( i = 0; i < n; i += 1 )
	{
		x += 1;

		// printf( 1, "spinner (pid = %d) ", getpid() );
		// printf( 1, "(x = %d)\n", x );
	}

	printf( 1, "spinner done! (pid = %d) ", getpid() );
	printf( 1, "(x = %d)\n", x );

	exit();
}


// wisc_spinner 50 &; wisc_spinner 77
