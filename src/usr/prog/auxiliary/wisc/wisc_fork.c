// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int pid;

	printf( stdout, "Hello 1\n" );

	pid = fork();

	// Child
	if ( pid == 0 )
	{
		printf( stdout, "child (pid = %d)\n", getpid() );
	}
	// Parent
	else if ( pid > 0 )
	{
		wait();

		printf( stdout, "parent (pid = %d)\n", getpid() );
	}

	printf( stdout, "Both reach here (pid = %d)\n", getpid() );

	exit();
}
