// http://pages.cs.wisc.edu/~cs537-3/Projects/p4b.html
// https://youtu.be/uMXGjayZ9DE

#include "kernel/types.h"
#include "user.h"

int stack [ 4096 ] __attribute__ ( ( aligned ( 4096 ) ) );
int x;

int main ( int argc, char* argv [] )
{
	x = 0;

	printf( 1, "Stack is at %p\n", stack );

	// int tid = fork();  // new process
	int tid = clone( stack );  // new thread

	if ( tid < 0 )
	{
		printf( 2, "error!\n" );
	}
	else if ( tid == 0 )  // child
	{
		for ( ;; )
		{
			x += 1;

			sleep( 100 );
		}
	}
	else  // parent
	{
		for ( ;; )
		{
			/*
				For threads, since share address space, when child
				modifies 'x' parent should see it.
			*/
			printf( 1, "x = %d\n", x );

			sleep( 100 );
		}
	}

	exit();
}
