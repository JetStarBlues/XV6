// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

int main ( int argc, char *argv[] )
{
	// TODO - fork without wait and zombie

	int pid;

	printf( 1, "Hello 1\n" );

	pid = fork();

	// Child
	if ( pid == 0 )
	{
		sleep( 200 );  // ticks

		printf( 1, "child (pid = %d)\n", getpid() );
	}
	// Parent
	else if ( pid > 0 )
	{
		// wait();

		printf( 1, "parent (pid = %d)\n", getpid() );
	}

	printf( 1, "Both reach here (pid = %d)\n", getpid() );

	exit();
}
