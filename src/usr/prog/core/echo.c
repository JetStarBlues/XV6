#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int i;

	for ( i = 1; i < argc; i += 1 )
	{
		printf( stdout, "%s%s", argv[ i ], i + 1 < argc ? " " : "\n" );
	}

	exit();
}
