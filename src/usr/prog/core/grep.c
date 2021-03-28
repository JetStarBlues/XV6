// Simple grep.

/* Code based on:
     The Practice of Programming, Kernighan & Pike, Chapter 9
*/

/*
Supports the follow operators:

	'^' - start of line
	'$' - end of line
	'.' - any single character
	'*' - zero or more occurrences

. A '^' or '$' that appears in the middle of a regular expression
  is treated as a literal character
*/

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"

#define MAXLINESZ 1023
#define NULL      0

static void grep      ( char*, int, char* );
static int  match     ( char*, char* );
static int  matchhere ( char*, char* );
static int  matchstar ( int, char*, char* );


int main ( int argc, char* argv [] )
{
	int   fd,
	      i;
	char* pattern;
	char* path;

	if ( argc <= 1 )
	{
		printf( stderr, "Usage: grep pattern [file ...]\n" );

		exit();
	}

	pattern = argv[ 1 ];

	// Look for pattern in stdin
	if ( argc <= 2 )
	{
		grep( pattern, stdin, NULL );

		exit();
	}

	// Look for pattern in listed files
	for ( i = 2; i < argc; i += 1 )
	{
		path = argv[ i ];

		fd = open( path, O_RDONLY );

		if ( fd < 0 )
		{
			printf( stderr, "grep: cannot open %s\n", path );

			// Skip problem file
			// continue;

			exit();
		}

		if ( argc == 3 )
		{
			grep( pattern, fd, NULL );
		}
		else
		{
			/* Print filename if more than one file is
			   being grep'ed for the pattern
			*/
			grep( pattern, fd, path );
		}

		close( fd );
	}

	exit();
}


// _________________________________________________________________________________

/* getbyte based on:
     The C Programming Language, Kernighan & Ritchie,
     2nd ed. Section 8.2
*/
/* Returns:
     byte on successful read
     (-1) on EOF
     (-2) on error
*/
#define INPUTBUFSZ  512

static int getbyte ( int fd )
{
	static char  buf [ INPUTBUFSZ ];
	static char* bufp;
	static int   n = 0;

	int ret;

	// Buffer is empty, fill it
	if ( n == 0 )
	{
		// Read bytes from fd
		n = read( fd, buf, INPUTBUFSZ );

		// Reached EOI, alert caller
		if ( n == 0 )
		{
			return - 1;  // (int) -1 falls outside char range...
		}
		// Error
		else if ( n < 0 )
		{
			// printf( stderr, "getbyte: read error\n" );

			return - 2;
		}

		// printf( stdout, "getbyte: read %d bytes\n", n );

		bufp = buf;
	}


	// Read from buffer
	ret = ( int ) ( ( unsigned char ) *bufp );

	bufp += 1;

	n -= 1;

	return ret;
}

/* Read in at most "size - 1" characters from fd and
   store them into buffer pointed to by s.

   Reading stops early an EOF or newline.
   If a newline is read, it is stored into the buffer.

   A terminating null byte is stored after the last
   character in the buffer.

   Returns s on success, and NULL on error or when
   EOF occurs while no characters have been read.
*/
static char* fgets ( char* s, int size, int fd )
{
	char* sbase;
	int   c;
	int   nToRead;

	sbase   = s;
	nToRead = size - 1;

	while ( nToRead )
	{
		c = getbyte( fd );

		if ( c < - 1 )  // error
		{
			return NULL;
		}
		// Reached EOF, done
		else if ( c == - 1 )
		{
			// Reached EOF without reading a char
			if ( s == sbase )
			{
				return NULL;
			}

			break;
		}
		else
		{
			*s = c;

			s += 1;

			// Read newline, done
			if ( c == '\n' )
			{
				break;
			}

			nToRead -= 1;
		}
	}

	*s = 0;  // null terminate

	return sbase;
}


// _________________________________________________________________________________

// Search for regexp in file
static void grep ( char* regexp, int fd, char* filename )
{
	int  slen;
	int  nmatch;
	char buf [ MAXLINESZ + 1 ];

	nmatch = 0;

	// For each line
	while ( fgets( buf, MAXLINESZ + 1, fd ) != NULL )
	{
		slen = strlen( buf );

		// Remove newline character
		if ( ( slen > 0 ) && ( buf[ slen - 1 ] == '\n' ) )
		{
			buf[ slen - 1 ] = 0;
		}

		// If matches, print the line
		if ( match( regexp, buf ) )
		{
			nmatch += 1;

			if ( filename != NULL )
			{
				printf( stdout, "%s:", filename );
			}

			printf( stdout, "%s\n", buf );
		}
	}
}

// Search for regexp anywhere in text
static int match ( char* regexp, char* text )
{
	/* If the regexp begins with '^', the text must begin
	   with a match of the remainder of the expression...
	*/
	if ( regexp[ 0 ] == '^' )
	{
		return matchhere( regexp + 1, text );
	}

	/* Otherwise, we walk along the text, to see
	   if the text matches at any position.

	   As soon as we find a match, we're done.
	*/

	// Must look even if string is empty
	while ( 1 )
	{
		if ( matchhere( regexp, text ) )
		{
			return 1;
		}

		if ( *text == '\0' )
		{
			break;
		}

		text += 1;  // next char in text
	}

	return 0;
}

// Search for regexp at beginning of text
static int matchhere ( char* regexp, char* text )
{
	/* If the regular expession is empty, we have reached
	   the end of it, and thus have found a match.
	*/
	if ( regexp[ 0 ] == '\0' )
	{
		return 1;
	}

	/* If the expression ends with '$', then we have found
	   a match only if the text is also at the end.
	*/
	if ( ( regexp[ 0 ] == '$' ) && ( regexp[ 1 ] == '\0' ) )
	{
		return *text == '\0';
	}

	/* Consider regexp "x*y":
	      We call matchstar with args:
	         . x     // current char in regexp
	         . y     // subsequent chars in regexp
	         . text  // 
	*/
	if ( regexp[ 1 ] == '*' )
	{
		return matchstar( regexp[ 0 ], regexp + 2, text );
	}

	/* Match any character or literal.
	   Recursive...
	*/
	if ( ( *text != '\0' ) &&
		 ( ( regexp[ 0 ] == '.' ) || ( regexp[ 0 ] == *text ) )
	   )
	{
		return matchhere( regexp + 1, text + 1 );
	}

	return 0;
}

// Search for c*regexp at beginning of text
static int matchstar ( int c, char* regexp, char* text )
{
	char* t;

	t = text;

	// Find all the text characters matching c (zero or more)
	while ( *t != 0 )
	{
		/* If we are not matching any character literal,
		   and *t does not match literal c, done
		*/
		if ( ( c != '.' ) && ( *t != c ) )
		{
			break;
		}

		t += 1;
	}

	// Match the rest of the regexp
	while ( 1 )
	{
		if ( matchhere( regexp, t ) )
		{
			return 1;
		}

		/* Back up if rest of string doesn't match pattern.
		   Maybe greedy consume ate some of the rest that would match.
		   For example:
		      re   = c.*dogs
		      ln   = crazydogs\0
		             0123456789
		      text = 1
		      t    = 9  // .* eats everything
		      Would need to back up t to 5
		*/
		if ( t <= text )
		{
			break;
		}

		t -= 1;
	}

	return 0;
}
