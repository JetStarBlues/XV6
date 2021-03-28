// https://youtu.be/YPIw67LuuY4

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int i,
	    x,
	    n;

	x = 0;
	n = atoi( argv[ 1 ] );  // get from command line

	for ( i = 0; i < n; i += 1 )
	{
		x += 1;

		printf( stdout, "spinner (pid = %d) (x = %d)\n", getpid(), x );
	}

	printf( stdout, "spinner done! (pid = %d) (x = %d)\n", getpid(), x );

	exit();
}


// wisc_spinner 50 &; wisc_spinner 77
