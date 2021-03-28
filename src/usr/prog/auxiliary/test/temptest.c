// https://youtu.be/2rAnCmXaOwo

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	printf( stdout, "abcdef\n" );
	// printf( stdout, "%c\n",   97 );
	printf( stdout, "%c\n",   'a' );
	printf( stdout, "%2c@\n",  'c' );
	printf( stdout, "%-5c@\n", 'f' );

	printf( stdout, "pasta\n" );

	printf( stdout, "%d\n",  123 );

	// printf( stdout, "%0d\n", 123 );
	printf( stdout, "%1d\n", 123 );
	printf( stdout, "%2d\n", 123 );
	printf( stdout, "%3d\n", 123 );
	printf( stdout, "%4d\n", 123 );
	printf( stdout, "%5d\n", 123 );
	printf( stdout, "%6d\n", 123 );

	printf( stdout, "%6d~~%6x\n", 123, 255 );

	printf( stdout, "%0d\n", 123 );
	printf( stdout, "%01d\n", 123 );
	printf( stdout, "%02d\n", 123 );
	printf( stdout, "%03d\n", 123 );
	printf( stdout, "%04d\n", 123 );
	printf( stdout, "%05d\n", 123 );
	printf( stdout, "%06d\n", 123 );

	// printf( stdout, "...\n" );
	// printf( stdout, "%06d\n", - 123 );
	// printf( stdout, "%6d\n",  - 123 );

	exit();
}
