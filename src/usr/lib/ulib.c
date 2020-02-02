#include "types.h"
#include "date.h"
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

void* memcpy ( void* dst, const void* src, uint n )
{
	const char* s;
	char*       d;

	s = src;
	d = dst;

	while ( n > 0 )
	{
		*d = *s;

		d += 1;
		s += 1;

		n -= 1;
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


// _________________________________________________________________

/* Returns zero if the two strings are equal.

   Returns a value less than zero, if first non equal
   character of 'p' is less than of 'q'. Ditto greater than.
   Possible use of the non-zero values is sorting.
*/
int strcmp ( const char* p, const char* q )
{
	while ( *p && ( *p == *q ) )
	{
		p += 1;
		q += 1;
	}

	return ( uchar ) *p - ( uchar ) *q;  // Why the cast (and not just "*p - *q")?
}

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

/* Copies the string pointed to by 'src', including the
   terminating null byte, to the buffer pointed to by 'dst'
*/
char* strcpy ( char* dst, const char* src )
{
	char* odst;

	odst = dst;

	while ( 1 )
	{
		*dst = *src;  // in case src is null string

		if ( *dst == 0 )
		{
			break;
		}

		dst += 1;
		src += 1;
	}

	return odst;
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

/* Returns a pointer to a new string which is a duplicate
   of string 's'.
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
   character 'c' in string 's'. If not found, returns NULL
*/
char* strchr ( const char* s, char c )
{
	while ( *s )
	{
		if ( *s == c )
		{
			return ( char* ) s;
		}

		s += 1;
	}

	return NULL;
}

/* Returns a pointer to the last occurrence of
   character 'c' in string 's'. If not found, returns NULL
*/
char* strrchr ( const char* s, char c )
{
	int slen;
	int i;

	slen = strlen( s );

	for ( i = slen - 1; i >= 0; i -= 1 )
	{
		if ( *( s + i ) == c )
		{
			return ( char* ) ( s + i );
		}
	}

	return NULL;
}

/* Returns a pointer to the beginning of the first occurrence
   of substring 'sub' in string 's'.
   If not found, returns NULl
*/ 
char* strstr ( const char* s, const char* sub )
{
	const char* subp;
	const char* ret;
	int         subLen;
	int         n;

	// Early exit if substring is empty
	if ( *sub == 0 )
	{
		return NULL;
	}

	//
	subLen = strlen( sub );

	//
	while ( *s )
	{
		// If matches first char of substring, check for full match
		if ( *s == *sub )
		{
			ret  = s;
			subp = sub;

			s    += 1;
			subp += 1;

			// Check if matches other chars
			n = 1;

			while ( *subp )
			{
				// Reached end of string without finding a match
				if ( *s == 0 )
				{
					return NULL;
				}

				// Mismatch before end of substring
				if ( *s != *subp )
				{
					break;
				}

				// Char matched, evaluate next
				n    += 1;
				s    += 1;
				subp += 1;
			}

			// If all chars matched, return pointer to beginning of match
			if ( n == subLen )
			{
				return ( char* ) ret;
			}
		}

		// Else, evaluate next char
		else
		{
			s += 1;
		}
	}

	// Reached end of string without finding a match
	return NULL;
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

// JK, crude implementation
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

	return ( int ) ( ( uchar ) c );
}

/* Description from man pages:

    Reads an entire line from 'fd', and stores the characters
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
	int   c;
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

		if ( c == - 1 )
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

			bPtr = *bufPtr + lineSize;  // update accordingly
		}


		// Add character to buffer
		*bPtr = ( char ) c;

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
