// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	printf( 1, "abcdef\n" );
	printf( 1, "%c\n",    97 );
	printf( 1, "%c\n",    'a' );
	printf( 1, "%2c@\n",  'c' );
	printf( 1, "%-5c@\n", 'f' );

	printf( 1, "pasta\n" );

	printf( 1, "%d\n",  123 );

	printf( 1, "%1d\n", 123 );
	printf( 1, "%2d\n", 123 );
	printf( 1, "%3d\n", 123 );
	printf( 1, "%4d\n", 123 );
	printf( 1, "%5d\n", 123 );
	printf( 1, "%6d\n", 123 );

	printf( 1, "%6d~~%6x\n", 123, 255 );
	printf( 1, "%6d~~%-6x%d\n", 123, 255, - 170 );

	printf( 1, "%0d\n",  123 );
	printf( 1, "%01d\n", 123 );
	printf( 1, "%02d\n", 123 );
	printf( 1, "%03d\n", 123 );
	printf( 1, "%04d\n", 123 );
	printf( 1, "%05d\n", 123 );
	printf( 1, "%06d\n", 123 );

	printf( 1, "...\n" );
	printf( 1, "%06d\n", - 123 );
	printf( 1, "%6d\n",  - 123 );
	printf( 1, "%06d\n",   123 );
	printf( 1, "%6d\n",    123 );

	printf( 1, "...\n" );
	printf( 1, "%02x\n", 250 );
	printf( 1, "%03x\n", 251 );
	printf( 1, "%04x\n", 252 );
	printf( 1, "%05x\n", 253 );
	printf( 1, "%6x\n",  254 );
	printf( 1, "%7x\n",  255 );

	printf( 1, "%02x %02x %02x %02x %02x %02x\n", 10, 11, 12, 13, 14, 15 );
	printf( 1, "%02x %02d %02c %02c %02x %02x %02d %s\n", 10, 11, '1', '2', 13, 14, 15, "<--" );

	exit();
}

/*
abcdef
a
a
 c@
f    @
pasta
123
123
123
123
 123
  123
   123
   123~~    FF
   123~~FF    -170
123
123
123
123
0123
00123
000123
...
-00123
  -123
000123
   123
...
FA
0FB
00FC
000FD
    FE
     FF
0A 0B 0C 0D 0E 0F
0A 11  1  2 0D 0E 15 <--
*/
