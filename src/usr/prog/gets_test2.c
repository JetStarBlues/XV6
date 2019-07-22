#include "types.h"
#include "user.h"

#define INPUTBUFSZ 20

int main ( int argc, char* argv [] )
{
	char buf [ INPUTBUFSZ ];

	printf( 1, "What's the secret phrase?\n" );

	while ( 1 )
	{
		memset( buf, 0, INPUTBUFSZ );

		gets( buf, INPUTBUFSZ );

		buf[ strlen( buf ) - 1 ] = 0;  // remove newline char '\n'

		if ( strcmp( buf, "beAns" ) == 0 )
		{
			break;
		}

		printf( 1, "Try again\n" );
	}

	printf( 1, "Correct!\n", buf );

	exit();
}
