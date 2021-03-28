#include "kernel/types.h"
#include "user.h"


int main ( int argc, char* argv [] )
{
	char* vowels;
	char* p;
	int*  numbers;
	int*  p2;

	// ---------------------------------------------------------

	vowels = malloc( 3 );

	vowels[ 0 ] = 'a';
	vowels[ 1 ] = 'e';
	vowels[ 2 ] = 'i';

	p = realloc( vowels, 5 );

	if ( p == NULL )
	{
		printf( stderr, "realloc failed\n" );
		exit();
	}

	vowels = p;

	vowels[ 3 ] = 'o';
	vowels[ 4 ] = 'u';

	printf( stdout,

		"vowels: %c%c%c%c%c\n",
		vowels[ 0 ],
		vowels[ 1 ],
		vowels[ 2 ],
		vowels[ 3 ],
		vowels[ 4 ]
	);

	// ---------------------------------------------------------

	numbers = malloc( 3 * sizeof( int ) );

	numbers[ 0 ] = 95;
	numbers[ 1 ] = 96;
	numbers[ 2 ] = 97;

	p2 = realloc( numbers, 5 * sizeof( int ) );

	if ( p2 == NULL )
	{
		printf( stderr, "realloc failed\n" );
		exit();
	}

	numbers = p2;

	numbers[ 3 ] = 98;
	numbers[ 4 ] = 99;

	printf( stdout,

		"numbers: %d %d %d %d %d \n",
		numbers[ 0 ],
		numbers[ 1 ],
		numbers[ 2 ],
		numbers[ 3 ],
		numbers[ 4 ]
	);


	free( vowels );
	free( numbers );

	exit();
}
