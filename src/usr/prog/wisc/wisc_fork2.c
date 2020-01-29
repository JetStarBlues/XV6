// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"
#include "fcntl.h"

int main ( int argc, char* argv [] )
{
	int   fd;
	int   pid;
	char* argv2 [ 2 ];

	printf( 1, "Hello 1\n" );

	close( 1 );  // close stdout

	// Note, slower than writing to stdout... hasn't frozen
	fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );

	//
	pid = fork();

	// Child
	if ( pid == 0 )
	{
		printf( 1, "child (pid = %d)\n", getpid() );

		argv2[ 0 ] = "ls";
		argv2[ 1 ] = 0;

		exec( argv2[ 0 ], argv2 );

		// Won't reach here as exec consumes child
	}
	// Parent
	else if ( pid > 0 )
	{
		wait();

		printf( 1, "parent (pid = %d)\n", getpid() );

		close( fd );
	}

	exit();
}
