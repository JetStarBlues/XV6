// init: The initial user-level program

#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
/* JK, following includes are needed to get
   the device number constants defined in file.h
   =(
*/
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

char* argv [] = { "sh", 0 };

int main ( void )
{
	int pid, wpid;

	// Create a new console device if needed, and open it
	// as stdin/out/error (fds 0, 1, 2)
	if ( open( "/dev/console", O_RDWR ) < 0 )
	{
		mknod( "/dev/console", CONSOLE, 0 );  // create a device file...

		open( "/dev/console", O_RDWR );  // stdin
	}

	dup( 0 );  // stdout
	dup( 0 );  // stderr


	/* For an example of multiple devices, see
	    https://github.com/DoctorWkt/xv6-freebsd/blob/master/cmd/old/init.c
	*/
	// Create a new display device if needed
	if ( open( "/dev/display", O_RDWR ) < 0 )
	{
		mknod( "/dev/display", DISPLAY, 0 );
	}


	/* Loops:
		- starts a shell in child process
		- handles orphaned processes until the shell exists
	*/
	for ( ;; )
	{
		printf( 1, "init: starting /bin/sh\n\n" );

		pid = fork();

		if ( pid < 0 )
		{
			printf( 2, "init: fork failed\n\n" );

			exit();
		}

		// Start shell in child
		if ( pid == 0 )
		{
			exec( "/bin/sh", argv );

			printf( 2, "init: exec /bin/sh failed\n\n" );

			exit();
		}


		// Adopt orphan processes
		/* If the shell has exited and there are still processes hanging
		   around, they must be abandoned.

		   We wait on all such processes...
		   wpid != 0 (sh), wpid != pid (init)

		   Why do we not wait for the shell (pid 0)?
		   If we cleanup while shell is running in child, we can handle its
		   abandoned children immediately ??
		*/
		/*
		   Only the parent (init) reaches this code section because
		   exec never returns if successful. If it fails, the child
		   calls 'exit', so it still won't reach here.
		*/
		while ( 1 )
		{
			wpid = wait();

			if ( ( wpid < 0 ) || ( wpid == pid ) )
			{
				break;  // ? To where?
			}

			printf( 1, "init: zombie!\n" );

			/* Why does init handle processes abandoned indirectly by child?
			   For example, why does it handle grandchildren abandoned by
			   sh's children?
			   Is this because of the while loop iterating through all
			   pids that are not 0 (sh) or itself (pid)?
			*/
			printf( 1, "  handled by (pid = %d)\n", getpid() );
		}
	}
}
