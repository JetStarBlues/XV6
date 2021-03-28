// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int fd;

	printf( stdout, "Hello 1\n" );


	fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );

	printf( fd, "Bonjour\n", fd );

	close( fd );


	// fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );
	fd = open( "output", O_CREATE | O_WRONLY | O_APPEND );

	printf( fd, "Bye\n", fd );

	close( fd );


	exit();
}
