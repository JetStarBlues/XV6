#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

// Returns 1 if a file is present in a directory,
// 0 otherwise

void exists ( char* filename, char* dirpath )
{
	int            fd,
	               i,
	               equal;
	char          *p;
	struct dirent  de;
	struct stat    st;


	if ( ( strlen( filename ) ) > DIRNAMESZ )
	{
		printf( 1, "invalid filename\n" );

		return;
	}

	if ( ( fd = open( dirpath, 0 ) ) < 0 )
	{
		printf( 2, "exists: cannot open %s\n", dirpath );

		return;
	}

	if ( fstat( fd, &st ) < 0 )
	{
		printf( 2, "exists: cannot stat %s\n", dirpath );

		close( fd );

		return;
	}

	if ( st.type == T_FILE )
	{
		printf( 1, "expecting a directory\n" );

		return;
	}

	if ( st.type == T_DIR )
	{
		while ( read( fd, &de, sizeof( de ) ) == sizeof( de ) )
		{
			if ( de.inum == 0 )  // ??
			{
				continue;
			}

			/* argv from sh is null terminated, thus filename
			    and dirpath are null terminated.
			   dirent->name is NOT null terminated
			*/

			// Compare the filename against dirent->name
			/* The two are equal if all characters up to the null terminal
			   in filename are equivalent.
			*/
			equal = 0;
			i     = 0;
			p     = filename;

			while ( 1 )
			{
				printf( 1, "%d : %c %d - %c %d\n", i, *p, *p, de.name[ i ], de.name[ i ] );

				if ( *p == 0 )  // reached null terminal
				{
					equal = 1;

					break;
				}

				if ( *p != de.name[ i ] )  // characters not equal
				{
					break;
				}

				i += 1;
				p += 1;
			}

			printf( 1, "\n" );

			// File found, we are done!
			if ( equal )
			{
				printf( 1, "file exists\n" );

				return;
			} 
		}

		// Not found
		printf( 1, "file does not exist\n" );	
	}

	close( fd );
}

int main ( int argc, char *argv [] )
{
	if ( ( argc < 2 ) || ( argc > 3 ) )
	{
		printf( 2, "Usage: exists filename dirpath\n" );

		exit();
	}

	if ( argc == 2 )
	{
		exists( argv[ 1 ], "." );
	}

	if ( argc == 3 )
	{
		exists( argv[ 1 ], argv[ 2 ] );
	}

	exit();
}
