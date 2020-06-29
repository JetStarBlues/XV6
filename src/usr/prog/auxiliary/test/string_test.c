#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	char* s;

	s = "Hello world sword!";

	printf( 1, "%s\n", strstr( s, "l"   ) );
	printf( 1, "%s\n", strstr( s, ""    ) );
	printf( 1, "%s\n", strstr( s, "wor" ) );

	exit();
}
