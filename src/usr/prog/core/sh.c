// Shell
/*
	_ Supported commands _______________

	cmd : description
	----:------------
	<   : redirect input ??
	>   : redirect output and truncate
	>>  : redirect output and append
	|   : pipe
	&   : fork a process. 'find &' will cause find to be forked and
	                       run in the background. It can be killed by its PID
	                       https://superuser.com/a/152695
	;   : delimiter, run multiple commands sequentially


	_ Supported combinations ___________

	cmd : description
	----:------------
	&;  : run multiple programs concurrently (each has own background process)
	&>  : run in background and redirect output

*/

/*
	https://pdos.csail.mit.edu/6.828/2018/homework/xv6-shell.html
		. Implement sub shells by implementing "(" and ")"
		. Implement running commands in the background by supporting "&" and "wait" 

	bourne shell commands : https://unix.stackexchange.com/a/159514
*/

/*
	TODO
		. Do we currently remove wrapper quotes before
		  passing as argument? For ex:

			echo "hello world"

			Does echo receive:
				"hello world"\0  or
				hello world\0

		. Standard behaviour seems to be to remove wrapper quotes...
		  and treat \" as literal
*/

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/date.h"  // shfind
#include "kernel/stat.h"  // shfind
#include "kernel/fs.h"    // shfind
#include "user.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

struct cmd
{
	int type;
};

struct execcmd
{
	int   type;
	char* argv  [ MAXARGS ];
	char* eargv [ MAXARGS ];
};

struct redircmd
{
	int         type;
	struct cmd* cmd;
	char*       file;
	char*       efile;
	int         mode;
	int         fd;
};

struct pipecmd
{
	int         type;
	struct cmd* left;
	struct cmd* right;
};

struct listcmd
{
	int         type;
	struct cmd* left;
	struct cmd* right;
};

struct backcmd
{
	int         type;
	struct cmd* cmd;
};

int         fork1    ( void );  // Fork but panics on failure.
void        panic    ( char* );
struct cmd* parsecmd ( char* );
int         shfind   ( char*, char*, char**, int );  // JK

// Execute cmd. Never returns.
void runcmd ( struct cmd* cmd )
{
	int              p [ 2 ];
	struct backcmd*  bcmd;
	struct execcmd*  ecmd;
	struct listcmd*  lcmd;
	struct pipecmd*  pcmd;
	struct redircmd* rcmd;
	char*            binLocation;  // JK

	if ( cmd == 0 )
	{
		exit();
	}

	switch ( cmd->type )
	{
		default:

			panic( "sh: runcmd" );


		case EXEC:

			ecmd = ( struct execcmd* ) cmd;

			if ( ecmd->argv[ 0 ] == 0 )
			{
				exit();
			}

			/* Executables are no longer all in root directory.

			   Based on,
			    https://github.com/DoctorWkt/xv6-freebsd/blob/d2a294c2a984baed27676068b15ed9a29b06ab6f/XV6_CHANGES#L124
			*/


			/* if a '/' is present, assume caller has specified
			   where to find the executable
			*/
			if ( strchr( ecmd->argv[ 0 ], '/' ) != NULL )
			{
				exec( ecmd->argv[ 0 ], ecmd->argv );

				printf( 2, "sh: exec %s failed\n", ecmd->argv[ 0 ] );
			}


			/* Executables without explicit path can either be in:
			      . "."      (non-recursive search)
			      . /bin     (recursive search)
			      . /usr/bin (recursive search)
			*/
			else
			{
				binLocation = NULL;

				// Binary in current directory
				shfind( ecmd->argv[ 0 ], ".", &binLocation, 0 );

				if ( binLocation != NULL )
				{
					exec( ecmd->argv[ 0 ], ecmd->argv );

					printf( 2, "sh: exec %s failed\n", ecmd->argv[ 0 ] );
				}

				else
				{
					// Binary in "/bin" (recursive search)
					shfind( ecmd->argv[ 0 ], "/bin", &binLocation, 1 );

					if ( binLocation != NULL )
					{
						exec( binLocation, ecmd->argv );

						printf( 2, "sh: exec %s failed\n", binLocation );
					}

					else
					{
						// Binary in "/usr/bin" (recursive search)
						shfind( ecmd->argv[ 0 ], "/usr/bin", &binLocation, 1 );

						if ( binLocation != NULL )
						{
							exec( binLocation, ecmd->argv );

							printf( 2, "sh: exec %s failed\n", binLocation );
						}

						else
						{
							// Binary not found
							printf( 2, "%s: command not found\n", ecmd->argv[ 0 ] );
						}
					}
				}
			}


			// Cleanup
			if ( binLocation != NULL )
			{
				free( binLocation );
			}

			//
			break;


		case REDIR:

			// Replace stdin/out/err with specified file,
			// by closing it and opening the file

			rcmd = ( struct redircmd* ) cmd;

			close( rcmd->fd );

			if ( open( rcmd->file, rcmd->mode ) < 0 )
			{
				printf( 2, "sh: open %s failed\n", rcmd->file );

				exit();
			}

			runcmd( rcmd->cmd );

			break;


		case PIPE:

			// Set stdout of left cmd to stdin of right cmd

			pcmd = ( struct pipecmd* ) cmd;

			if ( pipe( p ) < 0 )
			{
				panic( "sh: pipe" );
			}

			// Left cmd writes to pipe
			if ( fork1() == 0 )
			{
				// Set stdout to write end of pipe
				close( 1 );
				dup( p[ 1 ] );  /* First 'close' file descriptor 1 so that it becomes available.
				                   Then use 'dup' to:
				                     . duplicate the pipe's read end (struct file),
				                     . and assign it to the first available
				                       file descriptor, in this case 1 */

				// Close unused file descriptors
				close( p[ 0 ] );
				close( p[ 1 ] );

				runcmd( pcmd->left );
			}

			// Right cmd reads from pipe
			if ( fork1() == 0 )
			{
				// Set stdin to read end of pipe
				close( 0 );
				dup( p[ 0 ] );

				// Close unused file descriptors
				close( p[ 0 ] );
				close( p[ 1 ] );

				runcmd( pcmd->right );
			}

			// Close unused file descriptors
			close( p[ 0 ] );
			close( p[ 1 ] );

			wait();
			wait();

			break;


		case LIST:

			// Run multiple commands sequentially

			lcmd = ( struct listcmd* ) cmd;

			if ( fork1() == 0 )
			{
				runcmd( lcmd->left );
			}

			// Wait for completion before running next command
			wait();

			runcmd( lcmd->right );

			break;


		case BACK:  // background

			// Run command without waiting for its exit
			// The child returns immediately, while the grandchild continues running

			bcmd = ( struct backcmd* ) cmd;

			if ( fork1() == 0 )
			{
				runcmd( bcmd->cmd );
			}

			// wait();  // JK - How handle zombies? Using wait stops desired concurrency

			break;
	}

	exit();
}

int getcmd ( char* buf, int nbuf )
{
	printf( 2, "$ " );

	memset( buf, 0, nbuf );

	gets( buf, nbuf );

	if ( buf[ 0 ] == 0 )  // EOF
	{
		return - 1;
	}

	return 0;
}

int main ( void )
{
	static char buf [ 100 ];
	int         fd;

	// Ensure that three file descriptors are open
	fd = open( "/dev/console", O_RDWR );

	while ( fd >= 0 )
	{
		if ( fd >= 3 )
		{
			close( fd );

			break;
		}
	}

	// Read and run input commands.
	while ( getcmd( buf, sizeof( buf ) ) >= 0 )
	{
		/* 'cd' is the only command run by parent process.
		    Everything else is executed in a child process.
		*/
		if ( buf[ 0 ] == 'c' && buf[ 1 ] == 'd' && buf[ 2 ] == ' ' )
		{
			// Chdir must be called by the parent, not the child.
			buf[ strlen( buf ) - 1 ] = 0;  // chop \n

			if ( chdir( buf + 3 ) < 0 )
			{
				printf( 2, "sh: cannot cd %s\n", buf + 3 );
			}

			continue;
		}

		// Run command in child process
		if ( fork1() == 0 )
		{
			runcmd( parsecmd( buf ) );
		}

		wait();
	}

	exit();
}

void panic ( char* s )
{
	printf( 2, "%s\n", s );

	exit();
}

int fork1 ( void )
{
	int pid;

	pid = fork();

	if ( pid == - 1 )
	{
		panic( "sh: fork" );
	}

	return pid;
}


// __ Constructors ______________________________________________

struct cmd* execcmd ( void )
{
	struct execcmd* cmd;

	cmd = malloc( sizeof( *cmd ) );

	memset( cmd, 0, sizeof( *cmd ) );

	cmd->type = EXEC;

	return ( struct cmd* ) cmd;
}

struct cmd* redircmd ( struct cmd* subcmd, char* file, char* efile, int mode, int fd )
{
	struct redircmd* cmd;

	cmd = malloc( sizeof( *cmd ) );

	memset( cmd, 0, sizeof( *cmd ) );

	cmd->type  = REDIR;
	cmd->cmd   = subcmd;
	cmd->file  = file;
	cmd->efile = efile;
	cmd->mode  = mode;
	cmd->fd    = fd;

	return ( struct cmd* ) cmd;
}

struct cmd* pipecmd ( struct cmd* left, struct cmd* right )
{
	struct pipecmd* cmd;

	cmd = malloc( sizeof( *cmd ) );

	memset( cmd, 0, sizeof( *cmd ) );

	cmd->type  = PIPE;
	cmd->left  = left;
	cmd->right = right;

	return ( struct cmd* ) cmd;
}

struct cmd* listcmd ( struct cmd* left, struct cmd* right )
{
	struct listcmd* cmd;

	cmd = malloc( sizeof( *cmd ) );

	memset( cmd, 0, sizeof( *cmd ) );

	cmd->type  = LIST;
	cmd->left  = left;
	cmd->right = right;

	return ( struct cmd* ) cmd;
}

struct cmd* backcmd ( struct cmd* subcmd )
{
	struct backcmd* cmd;

	cmd = malloc( sizeof( *cmd ) );

	memset( cmd, 0, sizeof( *cmd ) );

	cmd->type = BACK;
	cmd->cmd  = subcmd;

	return ( struct cmd* ) cmd;
}


// __ Parsing ___________________________________________________

char whitespace [] = " \t\r\n\v";
char symbols []    = "<|>&;()";

int gettoken ( char** ps, char* es, char** q, char** eq )
{
	char* s;
	int   ret;

	s = *ps;

	while ( s < es && strchr( whitespace, *s ) )
	{
		s += 1;
	}

	if ( q )
	{
		*q = s;
	}

	ret = *s;

	switch ( *s )
	{
		case 0:

			break;

		case '|':
		case '(':
		case ')':
		case ';':
		case '&':
		case '<':

			s += 1;

			break;

		case '>':

			s += 1;

			if ( *s == '>' )
			{
				ret = '+';

				s += 1;
			}

			break;

		default:

			ret = 'a';

			while ( s < es && ! strchr( whitespace, *s ) && ! strchr( symbols, *s ) )
			{
				s += 1;
			}

			break;
	}

	if ( eq )
	{
		*eq = s;
	}

	while ( s < es && strchr( whitespace, *s ) )
	{
		s += 1;
	}

	*ps = s;

	return ret;
}

int peek ( char** ps, char* es, char* toks )
{
	char* s;

	s = *ps;

	while ( s < es && strchr( whitespace, *s ) )
	{
		s += 1;
	}

	*ps = s;

	return *s && strchr( toks, *s );
}

struct cmd* parseline    ( char**, char* );
struct cmd* parsepipe    ( char**, char* );
struct cmd* parseredirs  ( struct cmd*, char**, char* );
struct cmd* parseexec    ( char**, char* );
struct cmd* nulterminate ( struct cmd* );

struct cmd* parsecmd ( char* s )
{
	char        *es;
	struct cmd* cmd;

	es = s + strlen( s );

	cmd = parseline( &s, es );

	peek( &s, es, "" );

	if ( s != es )
	{
		/*printf( 2, "sh: leftovers - %s\n", s );  // ??

		panic( "sh: syntax" );*/

		printf( 2, "sh: leftovers - %s", s );  // ??

		panic( "" );
	}

	nulterminate( cmd );

	return cmd;
}

struct cmd* parseline ( char** ps, char* es )
{
	struct cmd* cmd;

	cmd = parsepipe( ps, es );

	while ( peek( ps, es, "&" ) )
	{
		gettoken( ps, es, 0, 0 );

		cmd = backcmd( cmd );
	}

	if ( peek( ps, es, ";" ) )
	{
		gettoken( ps, es, 0, 0 );

		cmd = listcmd( cmd, parseline( ps, es ) );
	}

	// JK
	else if ( peek( ps, es, "<>" ) )
	{
		cmd = parseredirs( cmd, ps, es );
	}

	return cmd;
}

struct cmd* parsepipe ( char** ps, char* es )
{
	struct cmd* cmd;

	cmd = parseexec( ps, es );

	if ( peek( ps, es, "|" ) )
	{
		gettoken( ps, es, 0, 0 );

		cmd = pipecmd( cmd, parsepipe( ps, es ) );
	}

	return cmd;
}

struct cmd* parseredirs ( struct cmd* cmd, char** ps, char* es )
{
	int   tok;
	char* q;
	char* eq;

	while ( peek( ps, es, "<>" ) )
	{
		tok = gettoken( ps, es, 0, 0 );

		if ( gettoken( ps, es, &q, &eq ) != 'a' )
		{
			panic( "sh: missing file for redirection" );
		}

		switch ( tok )
		{
			case '<':

				cmd = redircmd( cmd, q, eq, O_RDONLY, 0 );
				break;

			case '>':

				// cmd = redircmd( cmd, q, eq, O_WRONLY | O_CREATE, 1 );
				cmd = redircmd( cmd, q, eq, O_WRONLY | O_CREATE | O_TRUNC, 1 );  // JK
				break;

			case '+':  // >>

				// cmd = redircmd( cmd, q, eq, O_WRONLY | O_CREATE, 1 );
				cmd = redircmd( cmd, q, eq, O_WRONLY | O_CREATE | O_APPEND, 1 );  // JK
				break;
		}
	}

	return cmd;
}

struct cmd* parseblock ( char** ps, char* es )
{
	struct cmd* cmd;

	if ( ! peek( ps, es, "(" ) )
	{
		panic( "sh: parseblock" );
	}

	gettoken( ps, es, 0, 0 );

	cmd = parseline( ps, es );

	if ( ! peek( ps, es, ")" ) )
	{
		panic( "sh: syntax missing ')'" );
	}

	gettoken( ps, es, 0, 0 );

	cmd = parseredirs( cmd, ps, es );

	return cmd;
}

struct cmd* parseexec ( char** ps, char* es )
{
	char*           q;
	char*           eq;
	int             tok,
	                argc;
	struct execcmd* cmd;
	struct cmd*     ret;

	if ( peek( ps, es, "(" ) )
	{
		return parseblock( ps, es );
	}

	ret = execcmd();

	cmd = ( struct execcmd* ) ret;

	argc = 0;

	ret = parseredirs( ret, ps, es );

	while ( ! peek( ps, es, "|)&;" ) )
	{
		if ( ( tok = gettoken( ps, es, &q, &eq ) ) == 0 )
		{
			break;
		}

		if ( tok != 'a' )
		{
			panic( "sh: syntax" );  // ??
		}

		cmd->argv[ argc ] = q;

		cmd->eargv[ argc ] = eq;

		argc += 1;

		if ( argc >= MAXARGS )
		{
			panic( "sh: too many args" );
		}

		ret = parseredirs( ret, ps, es );
	}

	cmd->argv[ argc ] = 0;

	cmd->eargv[ argc ] = 0;

	return ret;
}

// NUL-terminate all the counted strings.
struct cmd* nulterminate ( struct cmd* cmd )
{
	int              i;
	struct backcmd*  bcmd;
	struct execcmd*  ecmd;
	struct listcmd*  lcmd;
	struct pipecmd*  pcmd;
	struct redircmd* rcmd;

	if ( cmd == 0 )
	{
		return 0;
	}

	switch ( cmd->type )
	{
		case EXEC:

			ecmd = ( struct execcmd* ) cmd;

			for ( i = 0; ecmd->argv[ i ]; i += 1 )
			{
				*ecmd->eargv[ i ] = 0;
			}

			break;

		case REDIR:

			rcmd = ( struct redircmd* ) cmd;

			nulterminate( rcmd->cmd );

			*rcmd->efile = 0;

			break;

		case PIPE:

			pcmd = ( struct pipecmd* ) cmd;

			nulterminate( pcmd->left );
			nulterminate( pcmd->right );

			break;

		case LIST:

			lcmd = ( struct listcmd* ) cmd;

			nulterminate( lcmd->left );
			nulterminate( lcmd->right );

			break;

		case BACK:

			bcmd = ( struct backcmd* ) cmd;

			nulterminate( bcmd->cmd );

			break;
	}

	return cmd;
}


// __ ... _______________________________________________________

#define SHFIND_DEBUG 0

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
