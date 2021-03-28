#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

/* By not explicitly closing files (including the pipe),
   we assume that they will be closed by our call to 'exit'.
   'exit' as part of its tasks, closes all the open
   file descriptors of the process.
*/

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

		printf( stdout, "child (send = %d)\n", send );		
	}
	// Parent
	else if ( pid > 0 )
	{
		int recv;

		wait();

		read( fds[ 0 ], &recv, sizeof( int ) );  // read from pipe

		printf( stdout, "parent (recv = %d)\n", recv );
	}

	exit();
}
