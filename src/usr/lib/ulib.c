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

void* memmove ( void* _dst, const void* _src, uint n )
{
	const char* src;
	char*       dst;

	dst = _dst;
	src = _src;

	while ( n > 0 )
	{
		/* *dst++ = *src++; */

		*dst = *src;

		dst += 1;
		src += 1;

		n -= 1;
	}

	return _dst;
}

void* memcpy ( void* dst, const void* src, uint n )
{
	return memmove( dst, src, n );
}



// _________________________________________________________________

/* Returns zero if the two strings are equal.

   Returns a value less than zero, if first non equal
   character of p is less than of q. Ditto greater than.
   Possible use of the non-zero values is sorting.
*/
int strcmp ( const char* p, const char* q )
{
	while ( *p && *p == *q )
	{
		p += 1;
		q += 1;
	}

	return ( uchar ) *p - ( uchar ) *q;  // Why the cast (and not just "*p - *q")?
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

// Length excluding null terminal
uint strlen ( const char* s )
{
	int n;

	for ( n = 0; s[ n ]; n += 1 )
	{
		//
	}

	return n;
}

/* Returns a pointer to the first occurrence of
   character c in string s. If not found, returns NULL
*/
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

/* Returns a pointer to a new string which is a duplicate
   of string s.
*/
char* strdup ( const char* s )
{
	int   slen;
	char* sdup;

	slen = strlen( s );

	sdup = ( char* ) malloc( slen + 1 );  // +1 for null terminal

	strcpy( sdup, s );

	return sdup;
}


// _________________________________________________________________

char* gets ( char* buf, int max )
{
	char c;
	int  nRead;
	int  i;

	for ( i = 0; ( i + 1 ) < max;  )
	{
		// Read one byte from stdin
		nRead = read( 0, &c, 1 );

		// Read error...
		if ( nRead < 1 )
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

// JK, crude implmentation
int getc ( int fd )
{
	char c;
	int  nRead;

	nRead = read( fd, &c, 1 );

	if ( nRead == 0 )  // EOF
	{
		return - 1;
	}
	else if ( nRead < 0 )  // error
	{
		return - 1;
	}

	return ( int ) c;
}

/* Description from man pages:

    Reads an entire line from 'fd', and stores the charcters
    into the buffer pointed to by '*bufPtr'.
    The buffer is null-terminated and includes any found
    newline characters.

    If '*bufPtr' is null, allocates a buffer to store the line,
    which should be freed by the caller.
    Updates '*bufPtr' and 'bufSize' accordingly.

    If 'bufSize' is not large enough to hold the line,
    it resizes '*bufPtr' with realloc.
    Updates '*bufPtr' and 'bufSize' accordingly.

    On success, returns the number of characters read, including
    newline characters, but not including the terminating null byte.

    On failure to read a line, (including EOF condition), returns -1.
*/
int getline ( char** bufPtr, uint* bufSize, int fd )
{
	char  c;
	int   lineSize;  // excluding null byte
	char* bPtr;
	void* p;


	if ( *bufPtr == NULL )
	{
		*bufSize = 128;  // arbitrary

		p = malloc( *bufSize );

		if ( p == NULL )
		{
			return - 1;
		}

		*bufPtr = p;
	}

	c        = 0;
	lineSize = 0;
	bPtr     = *bufPtr;

	while ( 1 )
	{
		// Get character from file
		c = getc( fd );

		if ( c < 0 )
		{
			return - 1;
		}


		// Grow buffer if not big enough
		if ( ( ( uint ) ( lineSize + 1 ) ) > *bufSize )
		{
			*bufSize *= 2;  // double

			p = realloc( *bufPtr, *bufSize );

			if ( p == NULL )
			{
				return - 1;
			}

			*bufPtr = p;
		}


		// Add character to buffer
		*bPtr = c;

		bPtr += 1;

		lineSize += 1;


		//
		if ( c == '\n' || c == '\r' )
		{
			break;
		}
	}

	// Null terminate
	*bPtr = 0;

	return lineSize;
}


// _________________________________________________________________

/*int putc ( int c, int fd )
{
	return write( fd, &c, 1 );
}*/


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
   Aka "ascii-to-integer"
*/
int atoi ( const char* s )
{
	int n;

	n = 0;

	while ( '0' <= *s && *s <= '9' )
	{
		n *= 10;

		n += *s - '0';

		s += 1;
	}

	return n;
}
