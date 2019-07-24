#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"


/* What are the implications of having same function twice
   (this version, and kernel version in 'string.c').
   What happens if user includes 'defs.h' and tries to
   use kernel version ??
*/
void* memset ( void* dst, int c, uint n )
{
	stosb( dst, c, n );

	return dst;
}

void* memmove ( void* vdst, const void* vsrc, int n )
{
	const char* src;
	char*       dst;

	dst = vdst;
	src = vsrc;

	while ( n > 0 )
	{
		/* *dst++ = *src++; */

		*dst = *src;

		dst += 1;
		src += 1;

		n -= 1;
	}

	return vdst;
}


// _________________________________________________________________

int strcmp ( const char* p, const char* q )
{
	while ( *p && *p == *q )
	{
		p += 1;
		q += 1;
	}

	return ( uchar ) *p - ( uchar ) *q;
}

/* Copies the string pointed to by src, including the
   terminating null byte, to the buffer pointed to by dst
*/
char* strcpy ( char* dst, const char* src )
{
	char* odst;

	odst = dst;

	/*while ( ( *dst++ = *src++ ) != 0 )
	{
		//
	}*/

	*dst = *src;  // in case src is null string

	while ( *dst )
	{
		dst += 1;
		src += 1;

		*dst = *src;
	}

	return odst;
}

uint strlen ( const char* s )
{
	int n;

	for ( n = 0; s[ n ]; n += 1 )
	{
		//
	}

	return n;
}

char* strchr ( const char* s, char c )
{
	for ( ; *s; s += 1 )
	{
		if ( *s == c )
		{
			return ( char* ) s;
		}
	}

	return 0;
}


// _________________________________________________________________

char* gets ( char* buf, int max )
{
	int  i,
	     cc;
	char c;

	for ( i = 0; ( i + 1 ) < max;  )
	{
		// Read one byte from stdin
		cc = read( 0, &c, 1 );

		// Read error...
		if ( cc < 1 )
		{
			break;
		}

		buf[ i ] = c;

		i += 1;

		if ( c == '\n' || c == '\r' )
		{
			break;
		}
	}

	buf[ i ] = '\0';

	return buf;
}


// _________________________________________________________________

int stat ( const char* path, struct stat* st )
{
	int fd;
	int r;

	fd = open( path, O_RDONLY );

	if ( fd < 0 )
	{
		return - 1;
	}

	r = fstat( fd, st );

	close( fd );

	return r;
}


// _________________________________________________________________

/* Converts the initial portion of the string to an int
*/
int atoi ( const char* s )
{
	int n;

	n = 0;

	while ( '0' <= *s && *s <= '9' )
	{
		/* n = n * 10 + *s++ - '0'; */

		n *= 10;

		n += *s - '0';

		s += 1;
	}

	return n;
}
