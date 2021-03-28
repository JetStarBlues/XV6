// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int   fd;
	int   pid;
	char* argv2 [ 2 ];

	printf( stdout, "Hello 1\n" );

	close( stdout );  // close stdout

	// Note, slower than writing to stdout... hasn't frozen
	fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );

	//
	pid = fork();

	// Child
	if ( pid == 0 )
	{
		printf( stdout, "child (pid = %d)\n", getpid() );

		argv2[ 0 ] = "ls";
		argv2[ 1 ] = 0;

		exec( argv2[ 0 ], argv2 );

		// Won't reach here as exec consumes child
	}
	// Parent
	else if ( pid > 0 )
	{
		wait();

		printf( stdout, "parent (pid = %d)\n", getpid() );

		close( fd );
	}

	exit();
}
