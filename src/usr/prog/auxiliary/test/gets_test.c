#include "kernel/types.h"
#include "user.h"

#define INPUTBUFSZ 20

int main ( int argc, char* argv [] )
{
	char buf [ INPUTBUFSZ ];

	printf( stdout, "Hi there!\nWhat's your name?\n" );

	// memset( buf, 0, INPUTBUFSZ );

	gets( buf, INPUTBUFSZ );

	buf[ strlen( buf ) - 1 ] = 0;  // remove newline char '\n'

	printf( stdout, "Pleasure to meet you %s!\n", buf );

	exit();
}
