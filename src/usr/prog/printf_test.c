// https://youtu.be/2rAnCmXaOwo

#include "types.h"
#include "user.h"

void test_printf ( void )
{
	printf( 1, "abcdef\n" );
	printf( 1, "%c\n",    97 );
	printf( 1, "%c\n",    'a' );
	printf( 1, "%2c@\n",  'c' );
	printf( 1, "%-5c@\n", 'f' );

	printf( 1, "%s\n", "pasta" );

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

}


/* sprintf test partially based on:
    http://joequery.me/code/snprintf-c/
*/
#define BUFSIZE 20

void init_buf ( char* buf )
{
	int i;

	// Initialize buffer to know state
	for ( i = 0; i < BUFSIZE; i += 1 )
	{
		buf[ i ] = '~';
	}
}

void print_buf ( char* buf )
{
	int  i;
	char c;

	for ( i = 0; i < BUFSIZE; i += 1 )
	{
		c = buf[ i ];

		if ( ( c >= 32 ) && ( c <= 126 ) )  // printable
		{
			printf( 1, "%c,", c );
		}
		else
		{
			printf( 1, "x%02x,", c );
		}
	}

	printf( 1, "\n" );
}

void test_sprintf ( void )
{
	int  ret;
	char s [ BUFSIZE ];

	init_buf( s );

	ret = sprintf( s, "string test" );
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "\n" );

	ret = snprintf( s, 3, "string test" );  // size parameter includes null byte
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "(size: %d, ret: %d)\n", 3, ret );
	printf( 1, "\n" );

	ret = snprintf( s, BUFSIZE, "cat" );  // "" less than BUFSIZE
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "(size: %d, ret: %d)\n", BUFSIZE, ret );
	printf( 1, "\n" );

	ret = snprintf( s, BUFSIZE, "there be great treachery" );  // "" larger than BUFSIZE
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "(size: %d, ret: %d)\n", BUFSIZE, ret );
	printf( 1, "\n" );


	/* Test formatting. Should also work if test_printf passes
	*/
	init_buf( s );

	ret = snprintf( s, BUFSIZE, "%02x %02x %02x %02x %02x %02x", 10, 11, 12, 13, 14, 15 );
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "(size: %d, ret: %d)\n", BUFSIZE, ret );
	printf( 1, "\n" );


	init_buf( s );

	ret = snprintf( s, BUFSIZE, "%02x %02d %02c %02c %02x %02x %02d %s", 10, 11, '1', '2', 13, 14, 15, "<--" );
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "(size: %d, ret: %d)\n", BUFSIZE, ret );
	printf( 1, "\n" );


	// Seems sprintf overflows silently :0
	ret = sprintf( s, "there be great treachery" );  // "" larger than BUFSIZE
	printf( 1, "%s\n", s );
	print_buf( s );
	printf( 1, "\n" );

/*
string test
s,t,r,i,n,g, ,t,e,s,t,x00,~,~,~,~,~,~,~,~,

st
s,t,x00,i,n,g, ,t,e,s,t,x00,~,~,~,~,~,~,~,~,
(size: 3, ret: 11)

cat
c,a,t,x00,n,g, ,t,e,s,t,x00,~,~,~,~,~,~,~,~,
(size: 20, ret: 3)

there be great trea
t,h,e,r,e, ,b,e, ,g,r,e,a,t, ,t,r,e,a,x00,
(size: 20, ret: 24)

0A 0B 0C 0D 0E 0F
0,A, ,0,B, ,0,C, ,0,D, ,0,E, ,0,F,x00,~,~,
(size: 20, ret: 17)

0A 11  1  2 0D 0E 1
0,A, ,1,1, , ,1, , ,2, ,0,D, ,0,E, ,1,x00,
(size: 20, ret: 24)
*/

}


void passABunchOfArgs ( int first, ... )
{
	va_list argp;

	va_start( argp, first );

	vprintf( 1, "%s %s %s\n", argp );

	va_end( argp );
}

void test_vprintf ( void )
{
	passABunchOfArgs( 0, "cats", "are", "cool", "and", "cute" );
}


int main ( int argc, char* argv [] )
{
	test_printf();
	test_sprintf();
	test_vprintf();

	exit();
}
