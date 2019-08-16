// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	printf( 1, "abcdef\n" );
	// printf( 1, "%c\n",   97 );
	printf( 1, "%c\n",   'a' );
	printf( 1, "%2c@\n",  'c' );
	printf( 1, "%-5c@\n", 'f' );

	printf( 1, "pasta\n" );

	printf( 1, "%d\n",  123 );

	// printf( 1, "%0d\n", 123 );
	printf( 1, "%1d\n", 123 );
	printf( 1, "%2d\n", 123 );
	printf( 1, "%3d\n", 123 );
	printf( 1, "%4d\n", 123 );
	printf( 1, "%5d\n", 123 );
	printf( 1, "%6d\n", 123 );

	printf( 1, "%6d~~%6x\n", 123, 255 );

	printf( 1, "%0d\n", 123 );
	printf( 1, "%01d\n", 123 );
	printf( 1, "%02d\n", 123 );
	printf( 1, "%03d\n", 123 );
	printf( 1, "%04d\n", 123 );
	printf( 1, "%05d\n", 123 );
	printf( 1, "%06d\n", 123 );

	// printf( 1, "...\n" );
	// printf( 1, "%06d\n", - 123 );
	// printf( 1, "%6d\n",  - 123 );

	exit();
}
