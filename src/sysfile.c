// File-system system calls.
// Mostly argument checking (since we don't trust
// user code) and calls into file.c and fs.c.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int argfd ( int n, int* pfd, struct file** pf )
{
	struct file* f;
	int          fd;

	if ( argint( n, &fd ) < 0 )
	{
		return - 1;
	}

	// Check that valid file descriptor
	if ( fd < 0 || fd >= NOFILE || ( f = myproc()->ofile[ fd ] ) == 0 )
	{
		return - 1;
	}

	// Return the fd
	if ( pfd )
	{
		*pfd = fd;
	}

	// Return corrsponding struct file
	if ( pf )
	{
		*pf = f;
	}

	return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int fdalloc ( struct file* f )
{
	struct proc* curproc = myproc();
	int          fd;

	for ( fd = 0; fd < NOFILE; fd += 1 )
	{
		if ( curproc->ofile[ fd ] == 0 )
		{
			curproc->ofile[ fd ] = f;

			return fd;
		}
	}

	return - 1;
}


// ___________________________________________________________________________

int sys_dup ( void )
{
	struct file* f;
	int          fd;

	if ( argfd( 0, 0, &f ) < 0 )
	{
		return - 1;
	}

	if ( ( fd = fdalloc( f ) ) < 0 )
	{
		return - 1;
	}

	filedup( f );

	return fd;
}

int sys_read ( void )
{
	struct file* f;
	int          n;
	char*        p;

	if ( argfd( 0, 0, &f )  < 0   ||
		 argint( 2, &n )    < 0   ||
		 argptr( 1, &p, n ) < 0 )
	{
		return - 1;
	}

	return fileread( f, p, n );
}

int sys_write ( void )
{
	struct file* f;
	int          n;
	char*        p;

	if ( argfd( 0, 0, &f )  < 0   ||
		 argint( 2, &n )    < 0   ||
		 argptr( 1, &p, n ) < 0 )
	{
		return - 1;
	}

	return filewrite( f, p, n );
}

int sys_close ( void )
{
	struct file* f;
	int          fd;

	if ( argfd( 0, &fd, &f ) < 0 )
	{
		return - 1;
	}

	myproc()->ofile[ fd ] = 0;

	fileclose( f );

	return 0;
}

int sys_fstat ( void )
{
	struct file* f;
	struct stat* st;

	if ( argfd( 0, 0, &f ) < 0 ||
		 argptr( 1, ( void* ) &st, sizeof( *st ) ) < 0 )
	{
		return - 1;
	}

	return filestat( f, st );
}


// ___________________________________________________________________________

// Create the path new as a link to the same inode as old.
int sys_link ( void )
{
	struct inode* dp;
	struct inode* ip;
	char          name [ DIRNAMESZ ];
	char*         new;
	char*         old;

	if ( argstr( 0, &old ) < 0 || argstr( 1, &new ) < 0 )
	{
		return - 1;
	}

	begin_op();

	if ( ( ip = namei( old ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	ilock( ip );

	if ( ip->type == T_DIR )
	{
		iunlockput( ip );

		end_op();

		return - 1;
	}

	ip->nlink += 1;

	iupdate( ip );

	iunlock( ip );

	if ( ( dp = nameiparent( new, name ) ) == 0 )
	{
		goto bad;
	}

	ilock( dp );

	if ( dp->dev != ip->dev || dirlink( dp, name, ip->inum ) < 0 )
	{
		iunlockput( dp );

		goto bad;
	}

	iunlockput( dp );

	iput( ip );

	end_op();

	return 0;

bad:

	ilock( ip );

	ip->nlink -= 1;

	iupdate( ip );

	iunlockput( ip );

	end_op();

	return - 1;
}

// Is the directory dp empty except for "." and ".." ?
static int isdirempty ( struct inode* dp )
{
	struct dirent de;
	int           off;

	for ( off = 2 * sizeof( de ); off < dp->size; off += sizeof( de ) )
	{
		if ( readi( dp, ( char* ) &de, off, sizeof( de ) ) != sizeof( de ) )
		{
			panic( "isdirempty: readi" );
		}

		if ( de.inum != 0 )
		{
			return 0;
		}
	}

	return 1;
}

int sys_unlink ( void )
{
	struct inode*  ip;
	struct inode*  dp;
	struct dirent  de;
	char           name [ DIRNAMESZ ];
	char*          path;
	uint           off;

	if ( argstr( 0, &path ) < 0 )
	{
		return - 1;
	}

	begin_op();

	if ( ( dp = nameiparent( path, name ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	ilock( dp );

	// Cannot unlink "." or "..".
	if ( namecmp( name, "." ) == 0 || namecmp( name, ".." ) == 0 )
	{
		goto bad;
	}

	if ( ( ip = dirlookup( dp, name, &off ) ) == 0 )
	{
		goto bad;
	}

	ilock( ip );

	if ( ip->nlink < 1 )
	{
		panic( "unlink: nlink < 1" );
	}

	if ( ip->type == T_DIR && ! isdirempty( ip ) )
	{
		iunlockput( ip );

		goto bad;
	}

	memset( &de, 0, sizeof( de ) );

	if ( writei( dp, ( char* ) &de, off, sizeof( de ) ) != sizeof( de ) )
	{
		panic( "unlink: writei" );
	}

	if ( ip->type == T_DIR )
	{
		dp->nlink -= 1;

		iupdate( dp );
	}

	iunlockput( dp );  // iput frees inode (and data) if this was last reference

	ip->nlink -= 1;

	iupdate( ip );

	iunlockput( ip );  // iput frees inode (and data) if this was last reference

	end_op();

	return 0;

bad:

	iunlockput( dp );

	end_op();

	return - 1;
}


// ___________________________________________________________________________

static struct inode* create ( char* path, short type, short major, short minor )
{
	struct inode* ip;
	struct inode* dp;
	uint          off;
	char          name [ DIRNAMESZ ];

	// If parent directory does not exist, fail
	if ( ( dp = nameiparent( path, name ) ) == 0 )
	{
		return 0;
	}

	ilock( dp );

	// If already exists,
	if ( ( ip = dirlookup( dp, name, &off ) ) != 0 )
	{
		iunlockput( dp );

		ilock( ip );

		// If it's a file and we wanted a file, return it
		if ( type == T_FILE && ip->type == T_FILE )
		{
			return ip;
		}

		iunlockput( ip );

		// Otherwise fail
		return 0;
	}

	// Does not exist, so let's create it
	if ( ( ip = ialloc( dp->dev, type ) ) == 0 )
	{
		panic( "create: ialloc" );
	}

	ilock( ip );

	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;

	iupdate( ip );

	if ( type == T_DIR )  // Create . and .. entries.
	{
		dp->nlink += 1;   // for ".."

		iupdate( dp );

		// No ip->nlink += 1 for ".": avoid cyclic ref count.

		if ( dirlink( ip, ".",  ip->inum ) < 0 ||  // current directory
			 dirlink( ip, "..", dp->inum ) < 0 )   // parent directory
		{
			panic( "create dots" );
		}
	}

	// And add it to the parent directory
	if ( dirlink( dp, name, ip->inum ) < 0 )
	{
		panic( "create: dirlink" );
	}

	iunlockput( dp );

	return ip;
}


// ___________________________________________________________________________

int sys_open ( void )
{
	struct inode* ip;
	struct file * f;
	char*         path;
	int           fd,
	              omode;
	uint          foffset;  // JK

	if ( argstr( 0, &path ) < 0 || argint( 1, &omode ) < 0 )
	{
		return - 1;
	}

	foffset = 0;

	begin_op();

	// Create if doesn't already exist
	if ( omode & O_CREATE )
	{
		ip = create( path, T_FILE, 0, 0 );

		if ( ip == 0 )
		{
			end_op();

			return - 1;
		}
	}
	// Lookup existing
	else
	{
		// If doesn't exist, fail
		if ( ( ip = namei( path ) ) == 0 )
		{
			end_op();
			
			return - 1;
		}

		ilock( ip );

		// Make sure only reading directories
		if ( ip->type == T_DIR && omode != O_RDONLY )
		{
			iunlockput( ip );

			end_op();

			return - 1;
		}
	}

	// JK
	if ( omode != O_RDONLY )
	{
		// If O_TRUNC, set file size to zero
		// (and free previously allocated data blocks)
		if ( ( omode & O_TRUNC ) && ( ip->type == T_FILE ) )
		{
			// cprintf( "O_TRUNC\n" );

			itrunc( ip );
		}

		// If O_APPEND, set file offset to EOF
		if ( ( omode & O_APPEND ) && ( ip->type != T_DIR ) )
		{
			// cprintf( "O_APPEND\n" );

			foffset = ip->size;
		}
	}

	// Allocate a file structure and file descriptor
	if ( ( f = filealloc() ) == 0 || ( fd = fdalloc( f ) ) < 0 )
	{
		if ( f )
		{
			fileclose( f );
		}

		iunlockput( ip );

		end_op();

		return - 1;
	}

	iunlock( ip );

	end_op();

	f->type     = FD_INODE;
	f->ip       = ip;
	f->off      = foffset;
	f->readable = ! ( omode & O_WRONLY );
	f->writable = ( omode & O_WRONLY ) || ( omode & O_RDWR );

	return fd;
}


// ___________________________________________________________________________

int sys_mknod ( void )
{
	struct inode* ip;
	char*         path;
	int           major,
	              minor;

	begin_op();

	if ( argstr( 0, &path )  < 0                             ||
		 argint( 1, &major ) < 0                             ||
		 argint( 2, &minor ) < 0                             ||
		 ( ip = create( path, T_DEV, major, minor ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	iunlockput( ip );

	end_op();

	return 0;
}

int sys_mkdir ( void )
{
	struct inode* ip;
	char*         path;

	begin_op();

	if ( argstr( 0, &path ) < 0 || ( ip = create( path, T_DIR, 0, 0 ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	iunlockput( ip );

	end_op();

	return 0;
}

int sys_chdir ( void )
{
	char*         path;
	struct inode* ip;
	struct proc*  curproc = myproc();
	
	begin_op();

	if ( argstr( 0, &path ) < 0 || ( ip = namei( path ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	ilock( ip );

	if ( ip->type != T_DIR )
	{
		iunlockput( ip );

		end_op();

		return - 1;
	}

	iunlock( ip );

	iput( curproc->cwd );

	end_op();

	curproc->cwd = ip;

	return 0;
}


// ___________________________________________________________________________

int sys_exec ( void )
{
	char* path;
	char* argv [ MAXARG ];
	int   i;
	uint  uargv,
	      uarg;

	if ( argstr( 0, &path ) < 0 || argint( 1, ( int* ) &uargv ) < 0 )
	{
		return - 1;
	}

	memset( argv, 0, sizeof( argv ) );

	for ( i = 0; ; i += 1 )
	{
		if ( i >= NELEM( argv ) )
		{
			return - 1;
		}

		if ( fetchint( uargv + 4 * i, ( int* ) &uarg ) < 0 )
		{
			return - 1;
		}

		if ( uarg == 0 )
		{
			argv[ i ] = 0;

			break;
		}

		if ( fetchstr( uarg, &argv[ i ] ) < 0 )
		{
			return - 1;
		}
	}

	return exec( path, argv );
}


// ___________________________________________________________________________

int sys_pipe ( void )
{
	struct file *rf;
	struct file *wf;
	int*         fd;
	int          fd0,
	             fd1;

	if ( argptr( 0, ( void* ) &fd, 2 * sizeof( fd[ 0 ] ) ) < 0 )
	{
		return - 1;
	}

	if ( pipealloc( &rf, &wf ) < 0 )
	{
		return - 1;
	}

	fd0 = - 1;

	if ( ( fd0 = fdalloc( rf ) ) < 0 || ( fd1 = fdalloc( wf ) ) < 0 )
	{
		if ( fd0 >= 0 )
		{
			myproc()->ofile[ fd0 ] = 0;
		}

		fileclose( rf );

		fileclose( wf );

		return - 1;
	}

	fd[ 0 ] = fd0;
	fd[ 1 ] = fd1;

	return 0;
}
