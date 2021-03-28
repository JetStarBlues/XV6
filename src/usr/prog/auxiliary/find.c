/* Finds if a file is present in the given directory

     . Available flags:
         . '-r', '--recursive' - search subdirectories
*/

#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/stat.h"
#include "kernel/fcntl.h"
#include "kernel/fs.h"
#include "user.h"


#define DEBUG 0

int stat2 ( char* path, struct stat* st )
{
	int fd;
	int ret;

	fd = open( path, O_RDONLY );

	if ( fd < 0 )
	{
		printf( stderr, "stat2: cannot open %s\n", path );

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

int find ( char* filename, char* dirpath, int recursiveSearch )
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
		printf( stderr, "find: invalid filename %s\n", filename );

		return - 1;
	}

	dirFd = open( dirpath, O_RDONLY );

	if ( dirFd < 0 )
	{
		printf( stderr, "find: cannot open %s\n", dirpath );

		return - 1;
	}

	if ( fstat( dirFd, &dirStat ) < 0 )
	{
		printf( stderr, "find: cannot fstat %s\n", dirpath );

		close( dirFd );

		return - 1;
	}

	if ( dirStat.type != T_DIR )
	{
		printf( stderr, "find: expecting second argument to be directory\n" );

		close( dirFd );

		return - 1;
	}


	if ( DEBUG )
	{
		printf( stdout, "%s\n", dirpath );
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

		if ( DEBUG )
		{
			printf( stdout, "\t%s\n", dirEntry.name );
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
			printf( stdout, "%s\n", dirEntryPath );

			foundMatch = 1;


			free( dirEntryPath );

			continue;
		}


		// Get info about the file
		if ( stat2( dirEntryPath, &dirEntryStat ) < 0 )
		{
			printf( stderr, "find: cannot stat2 %s\n", dirEntryPath );

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
				printf( stderr, "find: realloc failed\n" );

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
			foundMatch = find( filename, subDirectories[ i ], recursiveSearch );

			//
			if ( foundMatch == - 1 )  // error
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
	int i;
	int nRemainingArgs;
	int recursiveSearch;

	if ( argc < 2 )
	{
		goto badArgs;
	}


	//
	recursiveSearch = 0;
	i               = 1;

	// Consume flags
	while ( 1 )
	{
		if ( i >= argc )
		{
			goto badArgs;
		}

		if ( argv[ i ][ 0 ] != '-' )
		{
			break;
		}


		//
		if ( ( strcmp( "-r",          argv[ i ] ) == 0 )   ||
		     ( strcmp( "--recursive", argv[ i ] ) == 0 ) )
		{
			recursiveSearch = 1;
		}
		else
		{
			printf( stderr, "find: unknown flag (%s)\n", argv[ i ] );

			exit();
		}

		//
		i += 1;
	}


	// Consume filename and dirpath
	nRemainingArgs = argc - i + 1;

	if ( nRemainingArgs == 2 )
	{
		find( argv[ i ], ".", recursiveSearch );
	}
	else if ( nRemainingArgs == 3 )
	{
		find( argv[ i ], argv[ i + 1 ], recursiveSearch );
	}
	else
	{
		goto badArgs;
	}


	//
	exit();


badArgs:

	printf( stderr, "Usage: find [flags] filename [dirpath]\n" );

	exit();
}
