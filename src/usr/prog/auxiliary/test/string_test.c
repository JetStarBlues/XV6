#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	char* s;

	s = "Hello world sword!";

	printf( stdout, "%s\n", strstr( s, "l"   ) );
	printf( stdout, "%s\n", strstr( s, ""    ) );
	printf( stdout, "%s\n", strstr( s, "wor" ) );

	exit();
}
