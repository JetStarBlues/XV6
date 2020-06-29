/* Trigger a stack overflow
*/

#include "kernel/types.h"
#include "user.h"

#define SZ 4096  // 4096

void stackoverflow ( void )
{
	char array [ SZ ];

	array[ SZ - 1 ] = 255;

	printf( 1, "%d\n", array[ SZ - 1 ] );
}

int main ( int argc, char* argv [] )
{
	stackoverflow();

	exit();
}

/* See if writing to guard page...
*/
