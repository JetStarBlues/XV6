// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	// TODO - fork without wait and zombie

	int pid;

	printf( 1, "Hello 1\n" );

	pid = fork();

	// Child
	if ( pid == 0 )
	{
		// sleep( 200 );  // ticks

		// printf( 1, "child (pid = %d)\n", getpid() );


		int pid2;

		pid2 = fork();

		// These two run in parallel?

		// Grandchild
		if ( pid2 == 0 )
		{
			sleep( 200 );

			printf( 1, "grandchild (pid = %d)\n", getpid() );
		}
		else if ( pid2 > 0 )
		{
			printf( 1, "child (pid = %d)\n", getpid() );

			wait();  // wait for grandchild
		}
	}
	// Parent
	else if ( pid > 0 )
	{
		/* TODO, how to run concurrent properly?
		   i.e. parent doesn't wait for child to complete
		   but no zombies are left...
		*/

		wait();  // wait for child

		printf( 1, "parent (pid = %d)\n", getpid() );
	}

	printf( 1, "All reach here (pid = %d)\n", getpid() );

	exit();
}
