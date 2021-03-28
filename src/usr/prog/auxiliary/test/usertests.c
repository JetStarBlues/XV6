#include "kernel/param.h"
#include "kernel/types.h"
#include "kernel/date.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/syscall.h"
#include "kernel/trap.h"
#include "kernel/memlayout.h"
#include "kernel/mmu.h"
#include "user.h"

char          buf  [ 8192 ];
char          name [ 3 ];
unsigned long randstate   = 1;
char*         echoargv [] = { "echo", "cats", "are", "awesome!", 0 };


// What is the point of this?
unsigned int rand ()
{
	randstate = randstate * 1664525 + 1013904223;

	return randstate;
}

// does chdir() call iput( p->cwd ) in a transaction?
void iput_test ( void )
{
	printf( stdout, "iput test\n" );

	if ( mkdir( "iputdir" ) < 0 )
	{
		printf( stdout, "iput test: mkdir failed\n" );

		exit();
	}

	if ( chdir( "iputdir" ) < 0 )
	{
		printf( stdout, "iput test: chdir iputdir failed\n" );

		exit();
	}

	if ( unlink( "../iputdir" ) < 0 )
	{
		printf( stdout, "iput test: unlink ../iputdir failed\n" );

		exit();
	}

	if ( chdir( "/" ) < 0 )
	{
		printf( stdout, "iput test: chdir / failed\n" );

		exit();
	}

	printf( stdout, "iput test: OK\n" );
}

// does exit() call iput( p->cwd ) in a transaction?
void exitiput_test ( void )
{
	int pid;

	printf( stdout, "exit iput test\n" );

	pid = fork();

	if ( pid < 0 )
	{
		printf( stdout, "exit iput test: fork failed\n" );

		exit();
	}

	if ( pid == 0 )
	{
		if ( mkdir( "iputdir" ) < 0 )
		{
			printf( stdout, "exit iput test: mkdir failed\n" );

			exit();
		}

		if ( chdir( "iputdir" ) < 0 )
		{
			printf( stdout, "exit iput test: child chdir failed\n" );

			exit();
		}

		if ( unlink( "../iputdir" ) < 0 )
		{
			printf( stdout, "exit iput test: unlink ../iputdir failed\n" );

			exit();
		}

		exit();
	}

	wait();

	printf( stdout, "exit iput test: OK\n" );
}

/* Does the error path in open() for attempt to write a
   directory call iput() in a transaction?
   Needs a hacked kernel that pauses just after the namei()
   call in sys_open():
*/
/*	if ( ( ip = namei( path ) ) == 0 )
	{
		return - 1;
	}

	{
		int i;

		for ( i = 0; i < 10000; i += 1 )
		{
			yield();
		}
	}
*/

void openiput_test ( void )
{
	int pid;
	int fd;

	printf( stdout, "open iput test\n" );

	if ( mkdir( "oidir" ) < 0 )
	{
		printf( stdout, "open iput test: mkdir oidir failed\n" );

		exit();
	}

	pid = fork();

	if ( pid < 0 )
	{
		printf( stdout, "open iput test: fork failed\n" );

		exit();
	}

	if ( pid == 0 )
	{
		fd = open( "oidir", O_RDWR );

		if ( fd >= 0 )
		{
			printf( stdout, "open iput test: open directory for write succeeded\n" );

			exit();
		}

		exit();
	}

	sleep( 1 );

	if ( unlink( "oidir" ) != 0 )
	{
		printf( stdout, "open iput test: unlink failed\n" );

		exit();
	}

	wait();

	printf( stdout, "open iput test: OK\n" );
}

// simple file system tests

void open_test ( void )
{
	int fd;

	printf( stdout, "open test\n" );

	fd = open( "bin/echo", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "open test: open echo failed!\n" );

		exit();
	}

	close( fd );

	fd = open( "doesnotexist", 0 );

	if ( fd >= 0 )
	{
		printf( stdout, "open test: open doesnotexist succeeded!\n" );

		exit();
	}

	printf( stdout, "open test: OK\n" );
}

void smallfile_test ( void )
{
	int fd;
	int i;

	printf( stdout, "small file test\n" );

	fd = open( "small", O_CREATE | O_RDWR );

	if ( fd >= 0 )
	{
		printf( stdout, "small file test: creat small succeeded - OK\n" );
	}
	else
	{
		printf( stdout, "small file test: creat small failed!\n" );

		exit();
	}

	for ( i = 0; i < 100; i += 1 )
	{
		if ( write( fd, "aaaaaaaaaa", 10 ) != 10 )
		{
			printf( stdout, "small file test: write aa %d new file failed\n", i );

			exit();
		}

		if ( write( fd, "bbbbbbbbbb", 10 ) != 10 )
		{
			printf( stdout, "small file test: write bb %d new file failed\n", i );

			exit();
		}
	}

	printf( stdout, "small file test: writes - OK\n" );

	close( fd );

	fd = open( "small", O_RDONLY );

	if ( fd >= 0 )
	{
		printf( stdout, "small file test: open small succeeded - OK\n" );
	}
	else
	{
		printf( stdout, "small file test: open small failed!\n" );

		exit();
	}

	i = read( fd, buf, 2000 );

	if ( i == 2000 )
	{
		printf( stdout, "small file test: read succeeded - OK\n" );
	}
	else
	{
		printf( stdout, "small file test: read failed\n" );

		exit();
	}

	close( fd );

	if ( unlink( "small" ) < 0 )
	{
		printf( stdout, "small file test: unlink small failed\n" );

		exit();
	}

	printf( stdout, "small file test: OK\n" );
}

void bigfile_test ( void )
{
	int i,
	    fd,
	    n;

	printf( stdout, "big file test\n" );

	fd = open( "big", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "big file test: creat big failed!\n" );

		exit();
	}

	// Create largest file possible (i.e force allocation of all data blocks)
	for ( i = 0; i < MAXFILESZ; i += 1 )
	{
		( ( int* ) buf )[ 0 ] = i;

		if ( write( fd, buf, BLOCKSIZE ) != BLOCKSIZE )
		{
			printf( stdout, "big file test: write big file failed\n", i );

			exit();
		}
	}

	close( fd );

	fd = open( "big", O_RDONLY );

	if ( fd < 0 )
	{
		printf( stdout, "big file test: open big failed!\n" );

		exit();
	}

	n = 0;

	for ( ;; )
	{

		i = read( fd, buf, BLOCKSIZE );

		if ( i == 0 )
		{
			if ( n == MAXFILESZ - 1 )
			{
				printf( stdout, "big file test: read only %d blocks from big", n );

				exit();
			}

			break;
		}
		else if ( i != BLOCKSIZE )
		{
			printf( stdout, "big file test: read failed %d\n", i );

			exit();
		}

		if ( ( ( int* ) buf )[ 0 ] != n )
		{
			printf(

				stdout,
				"big file test: read content of block %d is %d\n",
				n,
				( ( int* ) buf )[ 0 ]
			);

			exit();
		}

		n += 1;
	}

	close( fd );

	if ( unlink( "big" ) < 0 )
	{
		printf( stdout, "big file test: unlink big failed\n" );

		exit();
	}

	printf( stdout, "big file test: OK\n" );
}

void create_test ( void )
{
	int i,
	    fd;

	printf( stdout, "many creates followed by unlink test\n" );

	name[ 0 ] = 'a';
	name[ 2 ] = '\0';

	for ( i = 0; i < 52; i += 1 )
	{
		name[ 1 ] = '0' + i;

		fd = open( name, O_CREATE | O_RDWR );

		close( fd );
	}

	name[ 0 ] = 'a';
	name[ 2 ] = '\0';

	for ( i = 0; i < 52; i += 1 )
	{
		name[ 1 ] = '0' + i;

		unlink( name );
	}

	printf( stdout, "many creates followed by unlink test: OK\n" );
}

void dir_test ( void )
{
	printf( stdout, "dir test\n" );

	if ( mkdir( "dir0" ) < 0 )
	{
		printf( stdout, "dir test: mkdir failed\n" );

		exit();
	}

	if ( chdir( "dir0" ) < 0 )
	{
		printf( stdout, "dir test: chdir dir0 failed\n" );

		exit();
	}

	if ( chdir( ".." ) < 0 )
	{
		printf( stdout, "dir test: chdir .. failed\n" );

		exit();
	}

	if ( unlink( "dir0" ) < 0 )
	{
		printf( stdout, "dir test: unlink dir0 failed\n" );

		exit();
	}

	printf( stdout, "dir test: OK\n" );
}

void exec_test ( void )
{
	int pid;

	printf( stdout, "exec test\n" );

	pid = fork();

	if ( pid == 0 )
	{
		if ( exec( "bin/echo", echoargv ) < 0 )
		{
			printf( stdout, "exec test: exec echo failed\n" );

			exit();
		}
	}
	else if ( pid > 0 )
	{
		wait();

		printf( stdout, "exec test: OK\n" );

	}
	else if ( pid < 0 )
	{
		printf( stdout, "exec test: fork failed\n" );

		exit();
	}
}

// simple fork and pipe read/write
void pipe_test ( void )
{
	int fds [ 2 ],
	    pid;
	int seq,
	    i,
	    n,
	    cc,
	    total;

	printf( stdout, "pipe test\n" );

	if ( pipe( fds ) != 0 )
	{
		printf( stdout, "pipe test: pipe() failed\n" );

		exit();
	}

	pid = fork();

	seq = 0;

	if ( pid == 0 )
	{
		close( fds[ 0 ] );

		for ( n = 0; n < 5; n += 1 )
		{
			for ( i = 0; i < 1033; i += 1 )
			{
				buf[ i ] = seq;

				seq += 1;
			}

			if ( write( fds[ 1 ], buf, 1033 ) != 1033 )
			{
				printf( stdout, "pipe test: oops 1\n" );

				exit();
			}
		}

		exit();
	}
	else if ( pid > 0 )
	{
		close( fds[ 1 ] );

		total = 0;

		cc = 1;

		while ( ( n = read( fds[ 0 ], buf, cc ) ) > 0 )
		{
			for ( i = 0; i < n; i += 1 )
			{
				if ( ( buf[ i ] & 0xff ) != ( seq++ & 0xff ) )
				{
					printf( stdout, "pipe test: oops 2\n" );

					return;
				}
			}

			total += n;

			cc = cc * 2;

			if ( cc > sizeof( buf ) )
			{
				cc = sizeof( buf );
			}
		}

		if ( total != 5 * 1033 )
		{
			printf( stdout, "pipe test: oops 3 total %d\n", total );

			exit();
		}

		close( fds[ 0 ] );

		wait();
	}
	else
	{
		printf( stdout, "pipe test: fork() failed\n" );

		exit();
	}

	printf( stdout, "pipe test: OK\n" );
}

// meant to be run w/ at most two CPUs
void preempt_test ( void )
{
	int pid1,
	    pid2,
	    pid3;
	int pfds [ 2 ];

	printf( stdout, "preempt test\n" );

	pid1 = fork();

	if ( pid1 == 0 )
	{
		for ( ;; )
		{
			//
		}
	}

	pid2 = fork();

	if ( pid2 == 0 )
	{
		for ( ;; )
		{
			//
		}
	}

	pipe( pfds );

	pid3 = fork();

	if ( pid3 == 0 )
	{
		close( pfds[ 0 ] );

		if ( write( pfds[ 1 ], "x", 1 ) != 1 )
		{
			printf( stdout, "preempt test: write error\n" );
		}

		close( pfds[ 1 ] );

		for ( ;; )
		{
			//
		}
	}

	close( pfds[ 1 ] );

	if ( read( pfds[ 0 ], buf, sizeof( buf ) ) != 1 )
	{
		printf( stdout, "preempt test: read error\n" );

		return;
	}

	close( pfds[ 0 ] );

	printf( stdout, "preempt test: killing...\n" );

	kill( pid1 );
	kill( pid2 );
	kill( pid3 );

	printf( stdout, "preempt test: waiting...\n" );

	wait();
	wait();
	wait();

	printf( stdout, "preempt test: OK\n" );
}

// try to find any races between exit and wait
void exitwait_test ( void )
{
	int i,
	    pid;

	printf( stdout, "exit wait test\n" );

	for ( i = 0; i < 100; i += 1 )
	{
		pid = fork();

		if ( pid < 0 )
		{
			printf( stdout, "exit wait test: fork failed\n" );

			return;
		}

		if ( pid )
		{
			if ( wait() != pid )
			{
				printf( stdout, "exit wait test: wait wrong pid\n" );

				return;
			}
		}
		else
		{
			exit();
		}
	}

	printf( stdout, "exit wait test: OK\n" );
}

void mem_test ( void )
{
	void*  m1;
	void*  m2;
	int    pid,
	       ppid;
	char** tmp;  // stackoverflow.com/a/57174081

	printf( stdout, "mem test\n" );

	ppid = getpid();

	pid = fork();

	if ( pid == 0 )
	{
		m1 = 0;

		while ( 1 )
		{
			m2 = malloc( 10001 );

			if ( m2 == 0 )
			{
				break;
			}

			// *( char** ) m2 = m1;

			tmp = ( char** ) m2;

			*tmp = m1;

			m1 = m2;
		}

		while ( m1 )
		{
			// m2 = *( char** ) m1;

			tmp = ( char** ) m1;

			m2 = *tmp;

			free( m1 );

			m1 = m2;
		}

		m1 = malloc( 1024 * 20 );

		if ( m1 == 0 )
		{
			printf( stdout, "mem test: couldn't allocate mem?!!\n" );

			kill( ppid );

			exit();
		}

		free( m1 );

		printf( stdout, "mem test: OK\n" );

		exit();
	}
	else
	{
		wait();
	}
}

// More file system tests

// two processes write to the same file descriptor
// is the offset shared? does inode locking work?
void sharedfd_test ( void )
{
	int  fd,
	     pid,
	     i,
	     n,
	     nc,
	     np;
	char buf [ 10 ];

	printf( stdout, "sharedfd test\n" );

	unlink( "sharedfd" );

	fd = open( "sharedfd", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "sharedfd test: cannot open sharedfd for writing" );

		return;
	}

	pid = fork();

	memset( buf, pid == 0 ? 'c' : 'p', sizeof( buf ) );

	for ( i = 0; i < 1000; i += 1 )
	{
		if ( write( fd, buf, sizeof( buf ) ) != sizeof( buf ) )
		{
			printf( stdout, "sharedfd test: write sharedfd failed\n" );

			break;
		}
	}

	if ( pid == 0 )
	{
		exit();
	}
	else
	{
		wait();
	}

	close( fd );

	fd = open( "sharedfd", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "sharedfd test: cannot open sharedfd for reading\n" );

		return;
	}

	nc = np = 0;

	while ( 1 )
	{
		n = read( fd, buf, sizeof( buf ) );

		if ( n <= 0 )
		{
			break;
		}

		for ( i = 0; i < sizeof( buf ); i += 1 )
		{
			if ( buf[ i ] == 'c' )
			{
				nc += 1;
			}
			if ( buf[ i ] == 'p' )
			{
				np += 1;
			}
		}
	}

	close( fd );

	unlink( "sharedfd" );

	if ( nc == 10000 && np == 10000 )
	{
		printf( stdout, "sharedfd test: OK\n" );
	}
	else
	{
		printf( stdout, "sharedfd test: oops %d %d\n", nc, np );

		exit();
	}
}

// four processes write different files at the same
// time, to test block allocation.
void fourfiles_test ( void )
{
	int   fd,
	      pid,
	      i,
	      j,
	      n,
	      total,
	      pi;
	char* names [] = { "f0", "f1", "f2", "f3" };
	char* fname;

	printf( stdout, "fourfiles test\n" );

	for ( pi = 0; pi < 4; pi += 1 )
	{
		fname = names[ pi ];

		unlink( fname );

		pid = fork();

		if ( pid < 0 )
		{
			printf( stdout, "fourfiles test: fork failed\n" );

			exit();
		}

		if ( pid == 0 )
		{
			fd = open( fname, O_CREATE | O_RDWR );

			if ( fd < 0 )
			{
				printf( stdout, "fourfiles test: create failed\n" );

				exit();
			}

			memset( buf, '0' + pi, BLOCKSIZE );

			for ( i = 0; i < 12; i += 1 )
			{
				n = write( fd, buf, 500 );

				if ( n != 500 )
				{
					printf( stdout, "fourfiles test: write failed %d\n", n );

					exit();
				}
			}

			exit();
		}
	}

	for ( pi = 0; pi < 4; pi += 1 )
	{
		wait();
	}

	for ( i = 0; i < 2; i += 1 )
	{
		fname = names[ i ];

		fd = open( fname, 0 );

		total = 0;

		while ( 1 )
		{
			n = read( fd, buf, sizeof( buf ) );

			if ( n <= 0 )
			{
				break;
			}

			for ( j = 0; j < n; j += 1 )
			{
				if ( buf[ j ] != '0' + i )
				{
					printf( stdout, "fourfiles test: wrong char\n" );

					exit();
				}
			}

			total += n;
		}

		close( fd );

		if ( total != ( 12 * 500 ) )
		{
			printf( stdout, "fourfiles test: wrong length %d\n", total );

			exit();
		}

		unlink( fname );
	}

	printf( stdout, "fourfiles test: OK\n" );
}

// four processes create and delete different files in same directory
void createdelete_test ( void )
{
	int  N,
	     pid,
	     i,
	     fd,
	     pi;
	char name [ 32 ];

	N = 20;

	printf( stdout, "create delete test\n" );

	for ( pi = 0; pi < 4; pi += 1 )
	{
		pid = fork();

		if ( pid < 0 )
		{
			printf( stdout, "create delete test: fork failed\n" );

			exit();
		}

		if ( pid == 0 )
		{
			name[ 0 ] = 'p' + pi;
			name[ 2 ] = '\0';

			for ( i = 0; i < N; i += 1 )
			{
				name[ 1 ] = '0' + i;

				fd = open( name, O_CREATE | O_RDWR );

				if ( fd < 0 )
				{
					printf( stdout, "create delete test: create failed\n" );

					exit();
				}

				close( fd );

				if ( i > 0 && ( i % 2  ) == 0 )
				{
					name[ 1 ] = '0' + ( i / 2 );

					if ( unlink( name ) < 0 )
					{
						printf( stdout, "create delete test: unlink failed\n" );

						exit();
					}
				}
			}

			exit();
		}
	}

	for ( pi = 0; pi < 4; pi += 1 )
	{
		wait();
	}

	name[ 0 ] = 0;
	name[ 1 ] = 0;
	name[ 2 ] = 0;

	for ( i = 0; i < N; i += 1 )
	{
		for ( pi = 0; pi < 4; pi += 1 )
		{
			name[ 0 ] = 'p' + pi;
			name[ 1 ] = '0' + i;

			fd = open( name, 0 );

			if ( ( i == 0 || i >= N / 2 ) && fd < 0 )
			{
				printf( stdout, "create delete test: oops, %s didn't exist\n", name );

				exit();
			}
			else if ( ( i >= 1 && i < N / 2 ) && fd >= 0 )
			{
				printf( stdout, "create delete test: oops, %s did exist\n", name );

				exit();
			}

			if ( fd >= 0 )
			{
				close( fd );
			}
		}
	}

	for ( i = 0; i < N; i += 1 )
	{
		for ( pi = 0; pi < 4; pi += 1 )
		{
			name[ 0 ] = 'p' + i;
			name[ 1 ] = '0' + i;

			unlink( name );
		}
	}

	printf( stdout, "create delete test: OK\n" );
}

// can I unlink a file and still read it?
void unlinkread_test ( void )
{
	int fd,
	    fd1;

	printf( stdout, "unlink read test\n" );

	fd = open( "unlinkread", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "unlink read test: create failed\n" );

		exit();
	}

	write( fd, "hello", 5 );

	close( fd );

	fd = open( "unlinkread", O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "unlink read test: open failed\n" );

		exit();
	}

	if ( unlink( "unlinkread" ) != 0 )
	{
		printf( stdout, "unlink read test: unlink failed\n" );

		exit();
	}

	fd1 = open( "unlinkread", O_CREATE | O_RDWR );

	write( fd1, "yyy", 3 );

	close( fd1 );

	if ( read( fd, buf, sizeof( buf ) ) != 5 )
	{
		printf( stdout, "unlink read test: read failed" );

		exit();
	}

	if ( buf[ 0 ] != 'h' )
	{
		printf( stdout, "unlink read test: wrong data\n" );

		exit();
	}

	if ( write( fd, buf, 10 ) != 10 )
	{
		printf( stdout, "unlink read test: write failed\n" );

		exit();
	}

	close( fd );

	unlink( "unlinkread" );

	printf( stdout, "unlink read test: OK\n" );
}

void link_test ( void )
{
	int fd;

	printf( stdout, "link test\n" );

	unlink( "lf1" );
	unlink( "lf2" );

	fd = open( "lf1", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "link test: create lf1 failed\n" );

		exit();
	}

	if ( write( fd, "hello", 5 ) != 5 )
	{
		printf( stdout, "link test: write lf1 failed\n" );

		exit();
	}

	close( fd );


	if ( link( "lf1", "lf2" ) < 0 )
	{
		printf( stdout, "link test: link lf1 lf2 failed\n" );

		exit();
	}

	unlink( "lf1" );

	if ( open( "lf1", 0 ) >= 0 )
	{
		printf( stdout, "link test: unlinked lf1 but it is still there!\n" );

		exit();
	}

	fd = open( "lf2", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "link test: open lf2 failed\n" );

		exit();
	}

	if ( read( fd, buf, sizeof( buf ) ) != 5 )
	{
		printf( stdout, "link test: read lf2 failed\n" );

		exit();
	}

	close( fd );

	if ( link( "lf2", "lf2" ) >= 0 )
	{
		printf( stdout, "link test: link lf2 lf2 succeeded! oops\n" );

		exit();
	}

	unlink( "lf2" );

	if ( link( "lf2", "lf1" ) >= 0 )
	{
		printf( stdout, "link test: link non-existant succeeded! oops\n" );

		exit();
	}

	if ( link( ".", "lf1" ) >= 0 )
	{
		printf( stdout, "link test: link . lf1 succeeded! oops\n" );

		exit();
	}

	printf( stdout, "link test: OK\n" );
}

// test concurrent create/link/unlink of the same file
void concurrentcreate_test ( void )
{
	int  i,
	     pid,
	     n,
	     fd;
	char file [ 3 ];
	char fa   [ 40 ];

	struct {

		ushort inum;
		char   name [ 14 ];

	} de;

	printf( stdout, "concurrent create test\n" );

	file[ 0 ] = 'C';
	file[ 2 ] = '\0';

	for ( i = 0; i < 40; i += 1 )
	{
		file[ 1 ] = '0' + i;

		unlink( file );

		pid = fork();

		if ( pid && ( i % 3 ) == 1 )
		{
			link( "C0", file );
		}
		else if ( pid == 0 && ( i % 5 ) == 1 )
		{
			link( "C0", file );
		}
		else
		{
			fd = open( file, O_CREATE | O_RDWR );

			if ( fd < 0 )
			{
				printf( stdout, "concurrent create test: %s failed\n", file );

				exit();
			}

			close( fd );
		}

		if ( pid == 0 )
		{
			exit();
		}
		else
		{
			wait();
		}
	}

	memset( fa, 0, sizeof( fa ) );

	fd = open( ".", 0 );

	n = 0;

	while ( read( fd, &de, sizeof( de ) ) > 0 )
	{
		if ( de.inum == 0 )
		{
			continue;
		}

		if ( de.name[ 0 ] == 'C' && de.name[ 2 ] == '\0' )
		{

			i = de.name[ 1 ] - '0';

			if ( i < 0 || i >= sizeof( fa ) )
			{
				printf( stdout, "concurrent create test: weird file %s\n", de.name );

				exit();
			}

			if ( fa[ i ] )
			{
				printf( stdout, "concurrent create test: duplicate file %s\n", de.name );

				exit();
			}

			fa[ i ] = 1;

			n += 1;
		}
	}

	close( fd );

	if ( n != 40 )
	{
		printf( stdout, "concurrent create test: not enough files in directory listing\n" );

		exit();
	}

	for ( i = 0; i < 40; i += 1 )
	{
		file[ 1 ] = '0' + i;

		pid = fork();

		if ( pid < 0 )
		{
			printf( stdout, "concurrent create test: fork failed\n" );

			exit();
		}

		if ( ( ( i % 3 ) == 0 && pid == 0 ) ||
			 ( ( i % 3 ) == 1 && pid != 0 ) )
		{
			close( open( file, 0 ) );
			close( open( file, 0 ) );
			close( open( file, 0 ) );
			close( open( file, 0 ) );
		}
		else
		{
			unlink( file );
			unlink( file );
			unlink( file );
			unlink( file );
		}

		if ( pid == 0 )
		{
			exit();
		}
		else
		{
			wait();
		}
	}

	printf( stdout, "concurrent create test: OK\n" );
}

// another concurrent link/unlink/create test,
// to look for deadlocks.
void linkunlink_test ()
{
	int pid,
	    i;

	printf( stdout, "link unlink test\n" );

	unlink( "x" );

	pid = fork();

	if ( pid < 0 )
	{
		printf( stdout, "link unlink test: fork failed\n" );

		exit();
	}

	unsigned int x = ( pid ? 1 : 97 );

	for ( i = 0; i < 100; i += 1 )
	{
		x = x * 1103515245 + 12345;

		if ( ( x % 3 ) == 0 )
		{
			close( open( "x", O_RDWR | O_CREATE ) );
		}
		else if ( ( x % 3 ) == 1 )
		{
			link( "cat", "x" );
		}
		else
		{
			unlink( "x" );
		}
	}

	if ( pid )
	{
		wait();
	}
	else
	{
		exit();
	}

	printf( stdout, "link unlink test: OK\n" );
}

// directory that uses indirect blocks
void bigdir_test ( void )
{
	int  i,
	     fd;
	char name [ 10 ];

	printf( stdout, "bigdir test\n" );

	unlink( "bd" );

	fd = open( "bd", O_CREATE );

	if ( fd < 0 )
	{
		printf( stdout, "bigdir test: create failed\n" );

		exit();
	}

	close( fd );

	for ( i = 0; i < 500; i += 1 )
	{
		name[ 0 ] = 'x';
		name[ 1 ] = '0' + ( i / 64 );
		name[ 2 ] = '0' + ( i % 64 );
		name[ 3 ] = '\0';

		if ( link( "bd", name ) != 0 )
		{
			printf( stdout, "bigdir test: link failed\n" );

			exit();
		}
	}

	unlink( "bd" );

	for ( i = 0; i < 500; i += 1 )
	{
		name[ 0 ] = 'x';
		name[ 1 ] = '0' + ( i / 64 );
		name[ 2 ] = '0' + ( i % 64 );
		name[ 3 ] = '\0';

		if ( unlink( name ) != 0 )
		{
			printf( stdout, "bigdir test: unlink failed" );

			exit();
		}
	}

	printf( stdout, "bigdir test: OK\n" );
}

void subdir_test ( void )
{
	int fd,
	    cc;

	printf( stdout, "subdir test\n" );

	unlink( "ff" );

	if ( mkdir( "dd" ) != 0 )
	{
		printf( stdout, "subdir test: mkdir dd failed\n" );

		exit();
	}

	fd = open( "dd/ff", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "subdir test: create dd/ff failed\n" );

		exit();
	}

	write( fd, "ff", 2 );

	close( fd );

	if ( unlink( "dd" ) >= 0 )
	{
		printf( stdout, "subdir test: unlink dd ( non-empty dir ) succeeded!\n" );

		exit();
	}

	if ( mkdir( "/dd/dd" ) != 0 )
	{
		printf( stdout, "subdir test: mkdir dd/dd failed\n" );

		exit();
	}

	fd = open( "dd/dd/ff", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "subdir test: create dd/dd/ff failed\n" );

		exit();
	}

	write( fd, "FF", 2 );

	close( fd );

	fd = open( "dd/dd/../ff", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "subdir test: open dd/dd/../ff failed\n" );

		exit();
	}

	cc = read( fd, buf, sizeof( buf ) );

	if ( ( cc != 2 ) || ( buf[ 0 ] != 'f' ) )
	{
		printf( stdout, "subdir test: dd/dd/../ff wrong content\n" );

		exit();
	}

	close( fd );

	if ( link( "dd/dd/ff", "dd/dd/ffff" ) != 0 )
	{
		printf( stdout, "subdir test: link dd/dd/ff dd/dd/ffff failed\n" );

		exit();
	}

	if ( unlink( "dd/dd/ff" ) != 0 )
	{
		printf( stdout, "subdir test: unlink dd/dd/ff failed\n" );

		exit();
	}

	if ( open( "dd/dd/ff", O_RDONLY ) >= 0 )
	{
		printf( stdout, "subdir test: open (unlinked) dd/dd/ff succeeded\n" );

		exit();
	}

	if ( chdir( "dd" ) != 0 )
	{
		printf( stdout, "subdir test: chdir dd failed\n" );

		exit();
	}

	if ( chdir( "dd/../../dd" ) != 0 )
	{
		printf( stdout, "subdir test: chdir dd/../../dd failed\n" );

		exit();
	}

	if ( chdir( "dd/../../../dd" ) != 0 )
	{
		printf( stdout, "subdir test: chdir dd/../../dd failed\n" );

		exit();
	}

	if ( chdir( "./.." ) != 0 )
	{
		printf( stdout, "subdir test: chdir ./.. failed\n" );

		exit();
	}

	fd = open( "dd/dd/ffff", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "subdir test: open dd/dd/ffff failed\n" );

		exit();
	}

	if ( read( fd, buf, sizeof( buf ) ) != 2 )
	{
		printf( stdout, "subdir test: read dd/dd/ffff wrong len\n" );

		exit();
	}

	close( fd );

	if ( open( "dd/dd/ff", O_RDONLY ) >= 0 )
	{
		printf( stdout, "subdir test: open (unlinked) dd/dd/ff succeeded!\n" );

		exit();
	}

	if ( open( "dd/ff/ff", O_CREATE | O_RDWR ) >= 0 )
	{
		printf( stdout, "subdir test: create dd/ff/ff succeeded!\n" );

		exit();
	}

	if ( open( "dd/xx/ff", O_CREATE | O_RDWR ) >= 0 )
	{
		printf( stdout, "subdir test: create dd/xx/ff succeeded!\n" );

		exit();
	}

	if ( open( "dd", O_CREATE ) >= 0 )
	{
		printf( stdout, "subdir test: create dd succeeded!\n" );

		exit();
	}
	
	if ( open( "dd", O_RDWR ) >= 0 )
	{
		printf( stdout, "subdir test: open dd rdwr succeeded!\n" );

		exit();
	}

	if ( open( "dd", O_WRONLY ) >= 0 )
	{
		printf( stdout, "subdir test: open dd wronly succeeded!\n" );

		exit();
	}

	if ( link( "dd/ff/ff", "dd/dd/xx" ) == 0 )
	{
		printf( stdout, "subdir test: link dd/ff/ff dd/dd/xx succeeded!\n" );

		exit();
	}

	if ( link( "dd/xx/ff", "dd/dd/xx" ) == 0 )
	{
		printf( stdout, "subdir test: link dd/xx/ff dd/dd/xx succeeded!\n" );

		exit();
	}

	if ( link( "dd/ff", "dd/dd/ffff" ) == 0 )
	{
		printf( stdout, "subdir test: link dd/ff dd/dd/ffff succeeded!\n" );

		exit();
	}

	if ( mkdir( "dd/ff/ff" ) == 0 )
	{
		printf( stdout, "subdir test: mkdir dd/ff/ff succeeded!\n" );

		exit();
	}

	if ( mkdir( "dd/xx/ff" ) == 0 )
	{
		printf( stdout, "subdir test: mkdir dd/xx/ff succeeded!\n" );

		exit();
	}

	if ( mkdir( "dd/dd/ffff" ) == 0 )
	{
		printf( stdout, "subdir test: mkdir dd/dd/ffff succeeded!\n" );

		exit();
	}

	if ( unlink( "dd/xx/ff" ) == 0 )
	{
		printf( stdout, "subdir test: unlink dd/xx/ff succeeded!\n" );

		exit();
	}

	if ( unlink( "dd/ff/ff" ) == 0 )
	{
		printf( stdout, "subdir test: unlink dd/ff/ff succeeded!\n" );

		exit();
	}

	if ( chdir( "dd/ff" ) == 0 )
	{
		printf( stdout, "subdir test: chdir dd/ff succeeded!\n" );

		exit();
	}

	if ( chdir( "dd/xx" ) == 0 )
	{
		printf( stdout, "subdir test: chdir dd/xx succeeded!\n" );

		exit();
	}

	if ( unlink( "dd/dd/ffff" ) != 0 )
	{
		printf( stdout, "subdir test: unlink dd/dd/ff failed\n" );

		exit();
	}

	if ( unlink( "dd/ff" ) != 0 )
	{
		printf( stdout, "subdir test: unlink dd/ff failed\n" );

		exit();
	}

	if ( unlink( "dd" ) == 0 )
	{
		printf( stdout, "subdir test: unlink non-empty dd succeeded!\n" );

		exit();
	}

	if ( unlink( "dd/dd" ) < 0 )
	{
		printf( stdout, "subdir test: unlink dd/dd failed\n" );

		exit();
	}

	if ( unlink( "dd" ) < 0 )
	{
		printf( stdout, "subdir test: unlink dd failed\n" );

		exit();
	}

	printf( stdout, "subdir test: OK\n" );
}

// test writes that are larger than the log.
void bigwrite_test ( void )
{
	int fd,
	    sz;

	printf( stdout, "big write test\n" );

	unlink( "bigwrite" );

	for ( sz = 499; sz < ( 12 * BLOCKSIZE ); sz += 471 )
	{
		fd = open( "bigwrite", O_CREATE | O_RDWR );

		if ( fd < 0 )
		{
			printf( stdout, "big write test: cannot create bigwrite\n" );

			exit();
		}

		int i;

		for ( i = 0; i < 2; i += 1 )
		{
			int cc = write( fd, buf, sz );

			if ( cc != sz )
			{
				printf( stdout, "big write test: write( %d ) ret %d\n", sz, cc );

				exit();
			}
		}

		close( fd );

		unlink( "bigwrite" );
	}

	printf( stdout, "big write test: OK\n" );
}

void bigfile_test2 ( void )
{
	int fd,
	    i,
	    total,
	    cc;

	printf( stdout, "big file test 2\n" );

	unlink( "bigfile" );

	fd = open( "bigfile", O_CREATE | O_RDWR );

	if ( fd < 0 )
	{
		printf( stdout, "big file test 2: cannot create bigfile" );

		exit();
	}

	for ( i = 0; i < 20; i += 1 )
	{

		memset( buf, i, 600 );

		if ( write( fd, buf, 600 ) != 600 )
		{
			printf( stdout, "big file test 2: write bigfile failed\n" );

			exit();
		}
	}

	close( fd );

	fd = open( "bigfile", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "big file test 2: cannot open bigfile\n" );

		exit();
	}

	total = 0;

	for ( i = 0; ; i += 1 )
	{
		cc = read( fd, buf, 300 );

		if ( cc < 0 )
		{
			printf( stdout, "big file test 2: read bigfile failed\n" );

			exit();
		}

		if ( cc == 0 )
		{
			break;
		}

		if ( cc != 300 )
		{
			printf( stdout, "big file test 2: short read bigfile\n" );

			exit();
		}

		if ( ( buf[ 0 ]   != i / 2 ) ||
		     ( buf[ 299 ] != i / 2 ) )
		{
			printf( stdout, "big file test 2: read bigfile wrong data\n" );

			exit();
		}

		total += cc;
	}

	close( fd );

	if ( total != 20 * 600 )
	{
		printf( stdout, "big file test 2: read bigfile wrong total\n" );

		exit();
	}

	unlink( "bigfile" );

	printf( stdout, "big file test 2: OK\n" );
}

// TODO: make this more generic
void fourteen_test ( void )
{
	int fd;

	// FILENAMESZ is 14.
	printf( stdout, "fourteen test\n" );

	if ( mkdir( "12345678901234" ) != 0 )
	{
		printf( stdout, "fourteen test: mkdir 12345678901234 failed\n" );

		exit();
	}

	if ( mkdir( "12345678901234/123456789012345" ) != 0 )
	{
		printf( stdout, "fourteen test: mkdir 12345678901234/123456789012345 failed\n" );

		exit();
	}

	fd = open( "123456789012345/123456789012345/123456789012345", O_CREATE );

	if ( fd < 0 )
	{
		printf( stdout, "fourteen test: create 123456789012345/123456789012345/123456789012345 failed\n" );

		exit();
	}

	close( fd );

	fd = open( "12345678901234/12345678901234/12345678901234", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "fourteen test: open 12345678901234/12345678901234/12345678901234 failed\n" );

		exit();
	}

	close( fd );

	if ( mkdir( "12345678901234/12345678901234" ) == 0 )
	{
		printf( stdout, "fourteen test: mkdir 12345678901234/12345678901234 succeeded!\n" );

		exit();
	}

	if ( mkdir( "123456789012345/12345678901234" ) == 0 )
	{
		printf( stdout, "fourteen test: mkdir 12345678901234/123456789012345 succeeded!\n" );

		exit();
	}

	printf( stdout, "fourteen test: OK\n" );
}

void rmdot_test ( void )
{
	printf( stdout, "rmdot test\n" );

	if ( mkdir( "dots" ) != 0 )
	{
		printf( stdout, "rmdot test: mkdir dots failed\n" );

		exit();
	}

	if ( chdir( "dots" ) != 0 )
	{
		printf( stdout, "rmdot test: chdir dots failed\n" );

		exit();
	}

	if ( unlink( "." ) == 0 )
	{
		printf( stdout, "rmdot test: rm . worked!\n" );

		exit();
	}

	if ( unlink( ".." ) == 0 )
	{
		printf( stdout, "rmdot test: rm .. worked!\n" );

		exit();
	}

	if ( chdir( "/" ) != 0 )
	{
		printf( stdout, "rmdot test: chdir / failed\n" );

		exit();
	}

	if ( unlink( "dots/." ) == 0 )
	{
		printf( stdout, "rmdot test: unlink dots/. worked!\n" );

		exit();
	}

	if ( unlink( "dots/.." ) == 0 )
	{
		printf( stdout, "rmdot test: unlink dots/.. worked!\n" );

		exit();
	}

	if ( unlink( "dots" ) != 0 )
	{
		printf( stdout, "rmdot test: unlink dots failed!\n" );

		exit();
	}

	printf( stdout, "rmdot test: OK\n" );
}

void dirfile_test ( void )
{
	int fd;

	printf( stdout, "dir vs file test\n" );

	fd = open( "dirfile", O_CREATE );

	if ( fd < 0 )
	{
		printf( stdout, "dir vs file test: create dirfile failed\n" );

		exit();
	}

	close( fd );

	if ( chdir( "dirfile" ) == 0 )
	{
		printf( stdout, "dir vs file test: chdir dirfile succeeded!\n" );

		exit();
	}

	fd = open( "dirfile/xx", 0 );

	if ( fd >= 0 )
	{
		printf( stdout, "dir vs file test: create dirfile/xx succeeded!\n" );

		exit();
	}

	fd = open( "dirfile/xx", O_CREATE );

	if ( fd >= 0 )
	{
		printf( stdout, "dir vs file test: create dirfile/xx succeeded!\n" );

		exit();
	}

	if ( mkdir( "dirfile/xx" ) == 0 )
	{
		printf( stdout, "dir vs file test: mkdir dirfile/xx succeeded!\n" );

		exit();
	}

	if ( unlink( "dirfile/xx" ) == 0 )
	{
		printf( stdout, "dir vs file test: unlink dirfile/xx succeeded!\n" );

		exit();
	}

	if ( link( "README", "dirfile/xx" ) == 0 )
	{
		printf( stdout, "dir vs file test: link to dirfile/xx succeeded!\n" );

		exit();
	}

	if ( unlink( "dirfile" ) != 0 )
	{
		printf( stdout, "dir vs file test: unlink dirfile failed!\n" );

		exit();
	}

	fd = open( ".", O_RDWR );

	if ( fd >= 0 )
	{
		printf( stdout, "dir vs file test: open . for writing succeeded!\n" );

		exit();
	}

	fd = open( ".", 0 );

	if ( write( fd, "x", 1 ) > 0 )
	{
		printf( stdout, "dir vs file test: write . succeeded!\n" );

		exit();
	}

	close( fd );

	printf( stdout, "dir vs file test: OK\n" );
}

// test that iput() is called at the end of _namei()
void emptyfilename_test ( void )
// void iref_test ( void )
{
	int i,
	    fd;

	printf( stdout, "empty file name test\n" );

	for ( i = 0; i < NINODE + 1; i += 1 )
	{
		if ( mkdir( "irefd" ) != 0 )
		{
			printf( stdout, "empty file name test: mkdir irefd failed\n" );

			exit();
		}

		if ( chdir( "irefd" ) != 0 )
		{
			printf( stdout, "empty file name test: chdir irefd failed\n" );

			exit();
		}


		mkdir( "" );

		link( "README", "" );

		fd = open( "", O_CREATE );

		if ( fd >= 0 )
		{
			close( fd );
		}

		fd = open( "xx", O_CREATE );

		if ( fd >= 0 )
		{
			close( fd );
		}

		unlink( "xx" );
	}

	chdir( "/" );

	printf( stdout, "empty file name test: OK\n" );
}

// test that fork fails gracefully
// the forktest binary also does this, but it runs out of proc entries first.
// inside the bigger usertests binary, we run out of memory first.
void fork_test ( void )
{
	int n,
	    pid;

	printf( stdout, "fork test\n" );

	for ( n = 0; n < 1000; n += 1 )
	{
		pid = fork();

		if ( pid < 0 )
		{
			break;
		}

		if ( pid == 0 )
		{
			exit();
		}
	}

	if ( n == 1000 )
	{
		printf( stdout, "fork test: fork claimed to work 1000 times!\n" );

		exit();
	}

	for ( ; n > 0; n -= 1 )
	{
		if ( wait() < 0 )
		{
			printf( stdout, "fork test: wait stopped early\n" );

			exit();
		}
	}

	if ( wait() != - 1 )
	{
		printf( stdout, "fork test: wait got too many\n" );

		exit();
	}

	printf( stdout, "fork test: OK\n" );
}

void sbrk_test ( void )
{
	int   fds [ 2 ],
	      pid,
	      pids [ 10 ],
	      ppid;
	char* a;
	char* b;
	char* c;
	char* lastaddr;
	char* oldbrk;
	char* p;
	char  scratch;
	uint  amt;

	printf( stdout, "sbrk test\n" );

	oldbrk = sbrk( 0 );

	// can one sbrk() less than a page?
	a = sbrk( 0 );

	int i;

	for ( i = 0; i < 5000; i += 1 )
	{
		b = sbrk( 1 );

		if ( b != a )
		{
			printf( stdout, "sbrk test: failed %d 0x%x 0x%x\n", i, a, b );

			exit();
		}

		*b = 1;

		a = b + 1;
	}

	pid = fork();

	if ( pid < 0 )
	{
		printf( stdout, "sbrk test: fork failed\n" );

		exit();
	}

	c = sbrk( 1 );
	c = sbrk( 1 );

	if ( c != a + 1 )
	{
		printf( stdout, "sbrk test: failed post-fork\n" );

		exit();
	}

	if ( pid == 0 )
	{
		exit();
	}

	wait();

	// can one grow address space to something big?
	#define BIG ( 100 * 1024 * 1024 )

	a = sbrk( 0 );

	amt = ( BIG ) - ( uint ) a;

	p = sbrk( amt );

	if ( p != a )
	{
		printf( stdout, "sbrk test: failed to grow big address space; enough phys mem?\n" );

		exit();
	}

	lastaddr = ( char* ) ( BIG - 1 );

	*lastaddr = 99;

	// can one de-allocate?
	a = sbrk( 0 );
	c = sbrk( - PGSIZE );

	if ( c == ( char* ) 0xffffffff )
	{
		printf( stdout, "sbrk test: could not deallocate\n" );

		exit();
	}

	c = sbrk( 0 );

	if ( c != a - PGSIZE )
	{
		printf( stdout, "sbrk test: deallocation produced wrong address, a 0x%x c 0x%x\n", a, c );

		exit();
	}

	// can one re-allocate that page?
	a = sbrk( 0 );
	c = sbrk( PGSIZE );

	if ( c != a || sbrk( 0 ) != a + PGSIZE )
	{
		printf( stdout, "sbrk test: re-allocation failed, a 0x%x c 0x%x\n", a, c );

		exit();
	}

	if ( *lastaddr == 99 )
	{
		// should be zero
		printf( stdout, "sbrk test: de-allocation didn't really deallocate\n" );

		exit();
	}

	a = sbrk( 0 );
	c = sbrk( - ( sbrk( 0 ) - oldbrk ) );

	if ( c != a )
	{
		printf( stdout, "sbrk test: downsize failed, a 0x%x c 0x%x\n", a, c );

		exit();
	}

	// can we read the kernel's memory?
	for ( a = ( char* ) ( KERNBASE ); a < ( char* ) ( KERNBASE + 2000000 ); a += 50000 )
	{
		ppid = getpid();

		pid = fork();

		if ( pid < 0 )
		{
			printf( stdout, "sbrk test: fork failed\n" );

			exit();
		}

		if ( pid == 0 )
		{
			printf( stdout, "sbrk test: oops could read 0x%x = 0x%x\n", a, *a );

			kill( ppid );

			exit();
		}

		wait();
	}

	// if we run the system out of memory, does it clean up the last
	// failed allocation?
	if ( pipe( fds ) != 0 )
	{
		printf( stdout, "sbrk test: pipe failed\n" );

		exit();
	}


	printf( stdout, "What are we doing here??\n" );

	for ( i = 0; i < sizeof( pids ) / sizeof( pids[ 0 ] ); i += 1 )
	{
		if ( ( pids[ i ] = fork() ) == 0 )
		{
			// allocate a lot of memory
			sbrk( BIG - ( uint ) sbrk( 0 ) );

			write( fds[ 1 ], "x", 1 );

			// sit around until killed
			for ( ;; )
			{
				sleep( 1000 );
			}
		}

		if ( pids[ i ] != - 1 )
		{
			read( fds[ 0 ], &scratch, 1 );
		}
	}


	printf( stdout, "What are we doing here 2??\n" );

	// if those failed allocations freed up the pages they did allocate,
	// we'll be able to allocate here
	c = sbrk( PGSIZE );

	for ( i = 0; i < sizeof( pids ) / sizeof( pids[ 0 ] ); i += 1 )
	{
		if ( pids[ i ] == - 1 )
		{
			continue;
		}

		kill( pids[ i ] );

		wait();
	}

	if ( c == ( char* ) 0xffffffff )
	{
		printf( stdout, "sbrk test: failed sbrk leaked memory\n" );

		exit();
	}

	if ( sbrk( 0 ) > oldbrk )
	{
		sbrk( - ( sbrk( 0 ) - oldbrk ) );
	}

	printf( stdout, "sbrk test: OK\n" );
}

void validateint ( int *p )
{
	int res;

	asm(

		"mov %%esp, %%ebx\n\t"
		"mov %3, %%esp\n\t"
		"int %2\n\t"
		"mov %%ebx, %%esp" :
		"=a" ( res ) :
		"a" ( SYS_sleep ), "n" ( T_SYSCALL ), "c" ( p ) :
		"ebx"
	);
}

void validate_test ( void )
{
	int  hi,
	     pid;
	uint p;

	printf( stdout, "validate test\n" );

	hi = 1100 * 1024;

	for ( p = 0; p <= ( uint )hi; p += PGSIZE )
	{
		pid = fork();

		if ( pid == 0 )
		{
			// try to crash the kernel by passing in a badly placed integer
			validateint( ( int* ) p );

			exit();
		}

		sleep( 0 );
		sleep( 0 );

		kill( pid );

		wait();

		// try to crash the kernel by passing in a bad string pointer
		if ( link( "nosuchfile", ( char* ) p ) != - 1 )
		{
			printf( stdout, "validate test: link should not succeed\n" );

			exit();
		}
	}

	printf( stdout, "validate test: OK\n" );
}

// does unintialized data start out zero?
char uninit [ 10000 ];

void bss_test ( void )
{
	int i;

	printf( stdout, "bss test\n" );

	for ( i = 0; i < sizeof( uninit ); i += 1 )
	{
		if ( uninit[ i ] != '\0' )
		{
			printf( stdout, "bss test: failed\n" );

			exit();
		}
	}

	printf( stdout, "bss test: OK\n" );
}

// does exec return an error if the arguments
// are larger than a page? or does it write
// below the stack and wreck the instructions/data?
void bigarg_test ( void )
{
	int pid,
	    fd;

	printf( stdout, "big arg test\n" );

	unlink( "bigarg-ok" );

	pid = fork();

	if ( pid == 0 )
	{
		static char* args [ MAXARG ];
		int          i;

		for ( i = 0; i < MAXARG - 1; i += 1 )
		{
			args[ i ] = "big arg test: failed\n                                                                                                                                                                                                       ";
		}

		args[ MAXARG - 1 ] = 0;

		exec( "bin/echo", args );

		printf( stdout, "big arg test: OK\n" );

		fd = open( "bigarg-ok", O_CREATE );

		close( fd );

		exit();
	}
	else if ( pid < 0 )
	{
		printf( stdout, "big arg test: fork failed\n" );

		exit();
	}

	wait();

	fd = open( "bigarg-ok", 0 );

	if ( fd < 0 )
	{
		printf( stdout, "big arg test: failed!\n" );

		exit();
	}

	close( fd );

	unlink( "bigarg-ok" );
}

// What happens when the file system runs out of blocks?
// answer: balloc panics, so this test is not useful.
void fsfull_test ()
{
	int nfiles;
	int fsblocks = 0;

	printf( stdout, "fsfull test\n" );

	for ( nfiles = 0; ; nfiles += 1 )
	{
		char name [ 64 ];

		name[ 0 ] = 'f';
		name[ 1 ] = '0' + nfiles / 1000;
		name[ 2 ] = '0' + ( nfiles % 1000 ) / 100;
		name[ 3 ] = '0' + ( nfiles % 100 ) / 10;
		name[ 4 ] = '0' + ( nfiles % 10 );
		name[ 5 ] = '\0';

		printf( stdout, "fsfull test: writing %s\n", name );

		int fd = open( name, O_CREATE | O_RDWR );

		if ( fd < 0 )
		{
			printf( stdout, "fsfull test: open %s failed\n", name );

			break;
		}

		int total = 0;

		while ( 1 )
		{
			int cc = write( fd, buf, BLOCKSIZE );

			if ( cc < BLOCKSIZE )
			{
				break;
			}

			total += cc;

			fsblocks += 1;
		}

		printf( stdout, "fsfull test: wrote %d bytes\n", total );

		close( fd );

		if ( total == 0 )
		{
			break;
		}
	}

	while ( nfiles >= 0 )
	{
		char name [ 64 ];

		name[ 0 ] = 'f';
		name[ 1 ] = '0' + nfiles / 1000;
		name[ 2 ] = '0' + ( nfiles % 1000 ) / 100;
		name[ 3 ] = '0' + ( nfiles % 100 ) / 10;
		name[ 4 ] = '0' + ( nfiles % 10 );
		name[ 5 ] = '\0';

		unlink( name );

		nfiles -= 1;
	}

	printf( stdout, "fsfull test: OK\n" );
}

void userio_test ()
{
	#define RTC_ADDR 0x70
	#define RTC_DATA 0x71

	ushort port = 0;
	uchar  val  = 0;
	int    pid;

	printf( stdout, "user io test\n" );

	pid = fork();

	if ( pid == 0 )
	{
		port = RTC_ADDR;

		val = 0x09;  /* year */

		/* http://wiki.osdev.org/Inline_Assembly/Examples */
		asm volatile( "outb %0,%1" : : "a"( val ), "d" ( port ) );

		port = RTC_DATA;

		asm volatile( "inb %1,%0" : "=a" ( val ) : "d" ( port ) );

		printf( stdout, "user io test: io succeeded, test failed\n" );

		exit();
	}
	else if ( pid < 0 )
	{
		printf ( 1, "user io test: fork failed\n" );

		exit();
	}

	wait();

	printf( stdout, "user io test: OK\n" );
}

void argp_test ()
{
	int fd;

	printf( stdout, "argp test\n" );

	fd = open( "bin/init", O_RDONLY );

	if ( fd < 0 )
	{
		printf( stderr, "argp test: open failed\n" );

		exit();
	}

	read( fd, sbrk( 0 ) - 1, - 1 );

	close( fd );

	printf( stdout, "argp test: OK\n" );
}


// _____________________________________________________________________________

int main ( int argc, char* argv[] )
{
	printf( stdout, "usertests starting\n" );

	// Tests require a fresh fs.img...
	if ( open( "usertests.ran", 0 ) >= 0 )
	{
		printf( stdout, "already ran user tests -- rebuild fs.img\n" );

		exit();
	}

	close( open( "usertests.ran", O_CREATE ) );


	/* TODO:
	   . sort in meaningful order
	   . also figure out what each is doing (write descriptions)
	*/
	exec_test();
	argp_test();
	createdelete_test();
	linkunlink_test();
	concurrentcreate_test();
	fourfiles_test();
	sharedfd_test();
	bigarg_test();
	bigwrite_test();
	bigarg_test();
	bss_test();
	sbrk_test();
	validate_test();
	open_test();
	smallfile_test();
	bigfile_test();
	create_test();

	openiput_test();
	exitiput_test();
	iput_test();

	mem_test();
	pipe_test();
	preempt_test();
	exitwait_test();
	rmdot_test();
	fourteen_test();  // TODO: make this more generic
	bigfile_test2();
	subdir_test();
	link_test();
	unlinkread_test();
	dirfile_test();
	emptyfilename_test();
	fork_test();
	bigdir_test();  // slow

	userio_test();

	// fsfull_test();  // not useful?

	printf( stdout, "ALL TESTS PASSED\n" );

	exit();
}
