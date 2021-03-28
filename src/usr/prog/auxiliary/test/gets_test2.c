#include "kernel/types.h"
#include "user.h"

#define INPUTBUFSZ 20

int main ( int argc, char* argv [] )
{
	char buf [ INPUTBUFSZ ];

	printf( stdout, "What's the secret phrase?\n" );

	while ( 1 )
	{
		memset( buf, 0, INPUTBUFSZ );

		gets( buf, INPUTBUFSZ );

		buf[ strlen( buf ) - 1 ] = 0;  // remove newline char '\n'

		if ( strcmp( buf, "beAns" ) == 0 )
		{
			break;
		}

		printf( stdout, "Try again\n" );
	}

	printf( stdout, "Correct!\n", buf );

	exit();
}
