// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	char* argv2 [ 2 ];

	printf( stdout, "Hello 1\n" );

	argv2[ 0 ] = "ls";
	argv2[ 1 ] = 0;

	exec( argv2[ 0 ], argv2 );

	printf( stdout, "Hello 2\n" );

	exit();
}
