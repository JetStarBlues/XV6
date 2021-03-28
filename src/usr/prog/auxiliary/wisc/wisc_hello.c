// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	printf( stdout, "Hello world!\n" );
	printf( stdout, "My pid is %d\n", getpid() );

	exit();
}
