// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"
#include "fcntl.h"

int main ( int argc, char *argv[] )
{
	int fd;

	printf( 1, "Hello 1\n" );


	fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );

	printf( fd, "Bonjour\n", fd );

	close( fd );


	// fd = open( "output", O_CREATE | O_WRONLY | O_TRUNC );
	fd = open( "output", O_CREATE | O_WRONLY | O_APPEND );

	printf( fd, "Bye\n", fd );

	close( fd );


	exit();
}
