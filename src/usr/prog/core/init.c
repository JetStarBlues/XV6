// init: The initial user-level program

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"
/* JK, following includes are needed to get
   the device number constants defined in file.h
   =(
*/
#include "kernel/spinlock.h"
#include "kernel/sleeplock.h"
#include "kernel/date.h"
#include "kernel/fs.h"
#include "kernel/file.h"

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
		printf( stdout, "init: starting /bin/sh\n\n" );

		pid = fork();

		if ( pid < 0 )
		{
			printf( stderr, "init: fork failed\n\n" );

			exit();
		}

		// Start shell in child
		if ( pid == 0 )
		{
			exec( "/bin/sh", argv );

			printf( stderr, "init: exec /bin/sh failed\n\n" );

			exit();
		}


		/* Wait on shell to exit...
		   In the meanwhile, adopt any orphan processes.
		*/
		/*
		   Only the parent (init) reaches this code section because
		   exec never returns if successful. If it fails, the child
		   calls 'exit', so it still won't reach here.
		*/
		while ( 1 )
		{
			wpid = wait();

			// All of our children have exited (immediate and adopted)...
			if ( wpid < 0 )
			{
				break;
			}

			// PID waited for was the shell we launched
			if ( wpid == pid )
			{
				break;
			}

			// PID waited for was abandoned by original parent
			/* init handles all abandoned children.
			   Why? Because if a process calls 'exit' while it still
			   has children, the children are passed on to init
			   ('p->parent = initproc')
			*/
			printf( stdout, "init: found abandoned zombie (pid = %d)!\n", wpid );
		}
	}
}
