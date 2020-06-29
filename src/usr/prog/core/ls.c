#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "time.h"

char* fmtname ( char* path )
{
	static char buf [ FILENAMESZ + 1 ];
	char*       p;
	int         slen;

	// Find first character after last slash.
	p = path + strlen( path );

	while ( ( p >= path ) && ( *p != '/' ) )
	{
		p -= 1;
	}

	p += 1;


	/* Possible at this point to just return p, and use
	   printf's format to add padding.
	   Assuming the printf implementation supports using
	   a variable (FILENAMESZ) in the format specifier:
	   https://www.geeksforgeeks.org/using-variable-format-specifier-c/
	*/


	slen = strlen( p );

	/* Return name */
	if ( slen == FILENAMESZ )
	{
		return p;  // already null-terminated...
	}

	/* Return blank-padded name */
	else if ( slen < FILENAMESZ )
	{
		// Copy name to buffer
		memmove( buf, p, slen );

		// Add trailing spaces to align
		memset( buf + slen, ' ', FILENAMESZ - slen );

		// Null terminate
		/* Since buf is static, its contents are assumed
		   to have been intialized to zero. Further since
		   the last char in buf is never updated, it is
		   assumed that it holds its initial value of zero.

		   TLDR - unnecessary to explicitly set null terminal
		*/
		// buf[ FILENAMESZ ] = 0;

		return buf;
	}

	/* Shouldn't be possible. 'open' would have failed */
	else
	{
		printf( 2, "ls: fmtname: unexpected filename %s\n", p );

		exit();
	}
}

void ls ( char* path )
{
	char           buf [ 512 ];
	char*          p;
	int            fd;
	struct dirent  de;
	struct stat    st;

	fd = open( path, O_RDONLY );

	if ( fd < 0 )
	{
		printf( 2, "ls: cannot open %s\n", path );

		return;
	}

	if ( fstat( fd, &st ) < 0 )
	{
		printf( 2, "ls: cannot stat %s\n", path );

		close( fd );

		return;
	}

	switch ( st.type )
	{
		case T_FILE:

			printf( 1, "\n" );  // Do nothing, ls was called on a file...

			break;

		case T_DIR:

			if ( strlen( path ) + 1 + FILENAMESZ + 1 > sizeof buf )
			{
				printf( 2, "ls: path too long\n" );

				break;
			}

			strcpy( buf, path );

			p = buf + strlen( buf );

			*p = '/';

			p += 1;

			// buf = "path/"
			//             ^
			//             p

			while ( read( fd, &de, sizeof( de ) ) == sizeof( de ) )
			{
				// Skip unused dirent
				if ( de.inum == 0 )
				{
					continue;
				}

				memmove( p, de.name, FILENAMESZ );

				p[ FILENAMESZ ] = 0;  // null terminate

				// buf = "path/dirname0"
				//             ^
				//             p

				if ( stat( buf, &st ) < 0 )
				{
					printf( 2, "ls: cannot stat %s\n", buf );

					continue;
				}

				printf( 1,

					"%s   %4s   %5d   %9d   "
					"%s %2d %4d %2d:%02d"
					"\n",
					fmtname( buf ),
					st.type == 1 ? "DIR" : st.type == 2 ? "FILE" : st.type == 3 ? "DEV" : "UND",
					st.ino, st.size,
					stringify_monthShort( st.mtime.month ), st.mtime.monthday, st.mtime.year, st.mtime.hour, st.mtime.minute
				);
			}

			break;
	}

	close( fd );
}

int main ( int argc, char* argv [] )
{
	int i;

	printf( 1, "%-14s | %4s | %5s | %9s | %17s\n", "name", "type", "inum", "size", "mtime" );
	printf( 1, "--------------------------------------------------------------\n" );

	if ( argc < 2 )
	{
		ls( "." );

		exit();
	}

	for ( i = 1; i < argc; i += 1 )
	{
		ls( argv[ i ] );
	}

	exit();
}
