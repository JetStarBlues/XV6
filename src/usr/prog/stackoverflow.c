/* Trigger a stack overflow
*/

#include "types.h"
#include "user.h"

#define SZ 5000  // 4096

int main ( int argc, char* argv [] )
{
	char array [ SZ ];

	array[ SZ - 1 ] = 255;

	printf( 1, "%d\n", array[ SZ - 1 ] );

	exit();
}

/* See if writing to guard page...
*/
