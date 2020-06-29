/* find optimized for sh's internal use:
     . stops at first match
     . returns pointer to string holding match path
*/

#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user.h"


#define SHFIND_DEBUG 1

int stat2 ( char* path, struct stat* st )
{
	int fd;
	int ret;

	fd = open( path, O_RDONLY );

	if ( fd < 0 )
	{
		printf( 2, "stat2: cannot open %s\n", path );

		return - 1;
	}

	ret = fstat( fd, st );

	close( fd );

	return ret;
}

void freeSubDirArray ( char** subDirectories, int nSubDirectories )
{
	int i;

	for ( i = 0; i < nSubDirectories; i += 1 )
	{
		free( subDirectories[ i ] );
	}

	free( subDirectories );
}

int shfind ( char* filename, char* dirpath, char** foundpath, int recursiveSearch )
{
	int            dirFd;
	struct dirent  dirEntry;
	struct stat    dirStat;
	struct stat    dirEntryStat;
	char           dirEntryName [ FILENAMESZ + 1 ];
	char*          dirEntryPath;
	int            dirpathLen;
	int            dirEntryNameLen;
	char**         subDirectories;
	int            nSubDirectories;
	char**         ptr;
	int            i;
	int            foundMatch;  // in current directory

	if ( strlen( filename ) > FILENAMESZ )
	{
		printf( 2, "shfind: invalid filename %s\n", filename );

		return - 1;
	}

	dirFd = open( dirpath, O_RDONLY );

	if ( dirFd < 0 )
	{
		printf( 2, "shfind: cannot open %s\n", dirpath );

		return - 1;
	}

	if ( fstat( dirFd, &dirStat ) < 0 )
	{
		printf( 2, "shfind: cannot fstat %s\n", dirpath );

		close( dirFd );

		return - 1;
	}

	if ( dirStat.type != T_DIR )
	{
		printf( 2, "shfind: expecting second argument to be directory\n" );

		close( dirFd );

		return - 1;
	}


	if ( SHFIND_DEBUG )
	{
		printf( 1, "%s\n", dirpath );
	}

	subDirectories  = NULL;
	nSubDirectories = 0;
	foundMatch      = 0;

	while ( read( dirFd, &dirEntry, sizeof( dirEntry ) ) == sizeof( dirEntry ) )
	{
		// Skip empty entry
		if ( dirEntry.inum == 0 )
		{
			continue;
		}

		if ( SHFIND_DEBUG )
		{
			printf( 1, "\t%s\n", dirEntry.name );
		}

		// Skip the "." entry
		if ( strcmp( dirEntry.name, "." ) == 0 )
		{
			continue;
		}

		// Skip the ".." entry
		if ( strcmp( dirEntry.name, ".." ) == 0 )
		{
			continue;
		}


		/* dirEntry->name is NOT guaranteed to be null terminated.
		   Let's null terminate it.
		*/
		memset( dirEntryName, 0, FILENAMESZ + 1 );
		memmove( dirEntryName, dirEntry.name, FILENAMESZ );


		// Get dirEntry's path...
		dirpathLen      = strlen( dirpath );
		dirEntryNameLen = strlen( dirEntryName );

		dirEntryPath = malloc( dirpathLen + 1 + dirEntryNameLen + 1 );

		memcpy( dirEntryPath, dirpath, dirpathLen );
		dirEntryPath[ dirpathLen ] = '/';
		memcpy( dirEntryPath + dirpathLen + 1, dirEntryName, dirEntryNameLen );
		dirEntryPath[ dirpathLen + 1 + dirEntryNameLen ] = 0;  // null terminate


		// Compare names
		if ( strcmp( dirEntryName, filename ) == 0 )
		{
			if ( SHFIND_DEBUG )
			{
				printf( 1, "%s\n", dirEntryPath );
			}

			foundMatch = 1;

			*foundpath = dirEntryPath;

			goto cleanup;
		}


		// Get info about the file
		if ( stat2( dirEntryPath, &dirEntryStat ) < 0 )
		{
			printf( 2, "shfind: cannot stat2 %s\n", dirEntryPath );

			foundMatch = - 1;  // error

			free( dirEntryPath );

			goto cleanup;
		}


		/* If file is a directory, save it to a list of subdirectories.

		   If we fail to find filename in the current directory,
		   the subdirectories in this list will be searched.
		*/
		if ( dirEntryStat.type == T_DIR )
		{
			//
			nSubDirectories += 1;


			//
			ptr = realloc( subDirectories, sizeof( char* ) * nSubDirectories );

			if ( ptr == NULL )
			{
				printf( 2, "shfind: realloc failed\n" );

				foundMatch = - 1;  // error

				free( dirEntryPath );

				goto cleanup;
			}

			subDirectories = ptr;


			//
			subDirectories[ nSubDirectories - 1 ] = dirEntryPath;
		}

		//
		else
		{
			free( dirEntryPath );

			continue;
		}
	}


	// Reached here without finding match in current directory...
	if ( recursiveSearch && ( nSubDirectories > 0 ) )
	{
		// Explore subdirectories
		for ( i = 0; i < nSubDirectories; i += 1 )
		{
			foundMatch = shfind( filename, subDirectories[ i ], foundpath, recursiveSearch );

			if ( foundMatch != 0 )
			{
				goto cleanup;
			}
		}
	}


cleanup:

	close( dirFd );

	freeSubDirArray( subDirectories, nSubDirectories );

	return foundMatch;
}

int main ( int argc, char* argv [] )
{
	char* match;
	int   recursiveSearch;

	if ( ( argc < 2 ) || ( argc > 3 ) )
	{
		printf( 2, "Usage: shfind filename dirpath\n" );

		exit();
	}

	match           = NULL;
	recursiveSearch = 1;

	if ( argc == 2 )
	{
		shfind( argv[ 1 ], ".", &match, recursiveSearch );
	}

	if ( argc == 3 )
	{
		shfind( argv[ 1 ], argv[ 2 ], &match, recursiveSearch );
	}

	printf( 1, "foundpath: %s\n", match );

	exit();
}
