// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	printf( 1, "Hello world!\n" );
	printf( 1, "My pid is %d\n", getpid() );

	exit();
}
