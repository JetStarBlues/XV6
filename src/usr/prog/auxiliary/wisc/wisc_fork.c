// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int pid;

	printf( 1, "Hello 1\n" );

	pid = fork();

	// Child
	if ( pid == 0 )
	{
		printf( 1, "child (pid = %d)\n", getpid() );
	}
	// Parent
	else if ( pid > 0 )
	{
		wait();

		printf( 1, "parent (pid = %d)\n", getpid() );
	}

	printf( 1, "Both reach here (pid = %d)\n", getpid() );

	exit();
}
