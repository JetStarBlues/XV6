#include "types.h"
#include "x86.h"

void* memset (  void* dst, int c, uint n  )
{
	// If possible, store 4 bytes at a time
	if ( ( ( ( int ) dst ) % 4 == 0 ) &&
		               ( n % 4 == 0 ) )
	{
		c &= 0xFF;

		stosl(

			dst,
			( c << 24 ) | ( c << 16 ) | ( c << 8 ) | c,
			n / 4
		);
	}

	// Otherwise, store 1 byte at a time
	else
	{
		stosb( dst, c, n );
	}

	return dst;
}

void* memmove ( void* dst, const void* src, uint n )
{
	const char* s;
	char*       d;

	s = src;
	d = dst;

	/* Check if src overlaps dst.

	     ...dddddd
	     ssssss...
	*/
	if ( ( s < d ) && ( ( s + n ) > d ) )
	{
		// Copy in reverse so that don't overwrite src before copy...
		s += n;
		d += n;

		while ( n > 0 )
		{
			d -= 1;
			s -= 1;

			*d = *s;

			n -= 1;
		}
	}

	// Otherwise, copy in "normal" direction...
	else
	{
		while ( n > 0 )
		{
			*d = *s;

			d += 1;
			s += 1;

			n -= 1;
		}
	}

	return dst;
}

// memcpy exists to placate GCC. Use memmove.
void* memcpy ( void* dst, const void* src, uint n )
{
	return memmove( dst, src, n );
}

int memcmp ( const void* src1, const void* src2, uint n )
{
	const uchar* s1;
	const uchar* s2;

	s1 = src1;
	s2 = src2;

	while ( n > 0 )
	{
		if ( *s1 != *s2 )
		{
			return *s1 - *s2;
		}

		s1 += 1;
		s2 += 1;

		n -= 1;
	}

	return 0;
}


// _________________________________________________________________

int strncmp ( const char* p, const char* q, int n )
{
	while ( ( n > 0 ) && ( *p ) && ( *p == *q ) )
	{
		n -= 1;
		p += 1;
		q += 1;
	}

	if ( n == 0 )
	{
		return 0;
	}

	return ( uchar ) *p - ( uchar ) *q;
}

char* strncpy ( char* dst, const char* src, int n )
{
	char* odst;

	odst = dst;

	while ( n > 0 )
	{
		*dst = *src;

		if ( *dst == 0 )
		{
			break;
		}

		dst += 1;
		src += 1;
		n   -= 1;
	}

	/* If length of 'src' is less than 'n', write additional
	   null bytes to 'dst'.
	*/
	while ( n > 0 )
	{
		*dst = 0;

		dst += 1;
		n   -= 1;
	}

	return odst;
}

// Like strncpy but guaranteed to NULL-terminate.
char* safestrcpy ( char* dst, const char* src, int n )
{
	char* odst;

	odst = dst;

	if ( n <= 0 )
	{
		return odst;
	}

	while ( n > 1 )  // >1 to reserve space for null terminal
	{
		*dst = *src;

		if ( *dst == 0 )
		{
			break;
		}

		dst += 1;
		src += 1;
		n   -= 1;
	}

	*dst = 0;  // null terminate

	return odst;
}

int strlen ( const char* s )
{
	int n;

	for ( n = 0; s[ n ]; n += 1 )
	{
		//
	}

	return n;
}
