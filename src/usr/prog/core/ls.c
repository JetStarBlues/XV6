#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"
#include "fcntl.h"

char* fmtname ( char* path )
{
	static char  buf [ DIRNAMESZ + 1 ];
	char*        p;

	// Find first character after last slash.
	for ( p = path + strlen( path ); p >= path && *p != '/'; p -= 1 )
	{
		//
	}

	p += 1;

	// Return blank-padded name.
	if ( strlen( p ) >= DIRNAMESZ )
	{
		return p;
	}

	memmove( buf, p, strlen( p ) );

	memset( buf + strlen( p ), ' ', DIRNAMESZ - strlen( p ) );  // pad with spaces to align

	return buf;
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

			if ( strlen( path ) + 1 + DIRNAMESZ + 1 > sizeof buf )
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
				if ( de.inum == 0 )  // ??
				{
					continue;
				}

				memmove( p, de.name, DIRNAMESZ );

				p[ DIRNAMESZ ] = 0;  // ?

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
