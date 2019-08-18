#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

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

	if ( ( fd = open( path, O_RDONLY ) ) < 0 )
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

			printf( 1, "%s %d %d %d\n", fmtname( path ), st.type, st.ino, st.size );

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

				printf( 1, "%s %d %d %d\n", fmtname( buf ), st.type, st.ino, st.size );
			}

			break;
	}

	close( fd );
}

int main ( int argc, char* argv [] )
{
	int i;

	printf( 1, "name | type | inum | size\n" );
	printf( 1, "-------------------------\n" );

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
