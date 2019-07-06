#include "types.h"
#include "user.h"
#include "fcntl.h"

int main ( int argc, char* argv [] )
{
	int fds [ 2 ];
	int pid;

	pipe( fds );

	pid = fork();

	// Child
	if ( pid == 0 )
	{
		int send;

		send = 123;

		write( fds[ 1 ], &send, sizeof( int ) );  // write to pipe

		printf( 1, "child (send = %d)\n", send );		
	}
	// Parent
	else if ( pid > 0 )
	{
		int recv;

		wait();

		read( fds[ 0 ], &recv, sizeof( int ) );  // read from pipe

		printf( 1, "parent (recv = %d)\n", recv );
	}

	exit();
}
