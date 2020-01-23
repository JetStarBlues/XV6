#include "types.h"
#include "date.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "fs.h"

void exists ( char* filename, char* dirpath )
{
	int   fd,
	      i,
	      equal;
	char* p;
	char  dename [ FILENAMESZ + 1 ];

	struct dirent  de;
	struct stat    st;


	if ( strlen( filename ) > FILENAMESZ )
	{
		printf( 2, "exists: invalid filename %s\n", filename );

		return;
	}

	fd = open( dirpath, O_RDONLY );

	if ( fd < 0 )
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
		printf( 2, "exists: expecting a directory\n" );

		close( fd );

		return;
	}

	if ( st.type == T_DIR )
	{
		while ( read( fd, &de, sizeof( de ) ) == sizeof( de ) )
		{
			if ( de.inum == 0 )  // empty directory entry
			{
				continue;
			}

			// Can potentially check if de refers to a regular file (T_FILE)

			/* argv from sh is null terminated, thus filename
			    and dirpath are null terminated.
			   dirent->name is NOT null terminated
			*/

			/* Not sure what value unused characters of dirent->name have.
			   Will be cautious and set them all to zero.
			   In the process, let's also "null terminate" dirent->name.
			*/
			memset( dename, 0, FILENAMESZ + 1 );
			memmove( dename, de.name, FILENAMESZ );

			// Compare the filename against dirent->name
			/* The two are equal if all characters up to the null terminal
			   in filename are equivalent.
			*/
			equal = 0;
			i     = 0;
			p     = filename;

			while ( 1 )
			{
				printf( 1, "%d : %c %d - %c %d\n", i, *p, *p, dename[ i ], dename[ i ] );

				if ( *p == 0 )  // reached null terminal
				{
					if ( dename[ i ] == 0 )
					{
						equal = 1;
					}

					break;
				}

				if ( *p != dename[ i ] )  // characters not equal
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

				close( fd );

				return;
			} 
		}
	}

	// Not found
	printf( 1, "file does not exist\n" );	

	close( fd );
}

int main ( int argc, char* argv [] )
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
