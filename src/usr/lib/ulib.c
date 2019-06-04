#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

char* strcpy ( char *s, const char *t )
{
	char *os;

	os = s;

	while ( ( *s++ = *t++ ) != 0 )
	{
		//
	}

	return os;
}

int strcmp ( const char *p, const char *q )
{
	while ( *p && *p == *q )
	{
		p += 1;
		q += 1;
	}

	return ( uchar )*p - ( uchar )*q;
}

uint strlen ( const char *s )
{
	int n;

	for ( n = 0; s[ n ]; n += 1 )
	{
		//
	}

	return n;
}

char* strchr ( const char *s, char c )
{
	for ( ; *s; s += 1 )
	{
		if ( *s == c )
		{
			return ( char* )s;
		}
	}

	return 0;
}

char* gets ( char *buf, int max )
{
	int  i,
	     cc;
	char c;

	for ( i = 0; i + 1 < max;  )
	{
		cc = read( 0, &c, 1 );

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

int stat ( const char *path, struct stat *st )
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

int atoi ( const char *s )
{
	int n;

	n = 0;

	while ( '0' <= *s && *s <= '9' )
	{
		n = n * 10 + *s++ - '0';
	}

	return n;
}

void* memmove ( void *vdst, const void *vsrc, int n )
{
	const char *src;
	char       *dst;

	dst = vdst;
	src = vsrc;

	while ( n > 0 )
	{
		*dst++ = *src++;

		n -= 1;
	}

	return vdst;
}

void* memset ( void *dst, int c, uint n )
{
	stosb( dst, c, n );

	return dst;
}
