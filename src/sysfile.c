// File-system system calls.
// Mostly argument checking (since we don't trust user code)
// and calls into file.c and fs.c.

/* With the functions that the lower layers of the file system
   provide, the implementation of most system calls is trivial.
   However a few require attention...
     . sys_link
     . sys_unlink
     . sys_open
*/

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
	if ( fd < 0 || fd >= NOPENFILE_PROC || ( f = myproc()->ofile[ fd ] ) == 0 )
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
/*
   Allocates the first available file descriptor in the
   current process's list of file descriptors...
*/
static int fdalloc ( struct file* f )
{
	struct proc* curproc = myproc();
	int          fd;

	for ( fd = 0; fd < NOPENFILE_PROC; fd += 1 )
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

// Create the path 'newpath' as a link to the same inode as path 'oldpath'
int sys_link ( void )
{
	struct inode* dir;
	struct inode* ip;
	char          name [ FILENAMESZ ];
	char*         newpath;
	char*         oldpath;

	// Fetch arguments
	if ( argstr( 0, &oldpath ) < 0 || argstr( 1, &newpath ) < 0 )
	{
		return - 1;
	}

	begin_op();

	if ( ( ip = namei( oldpath ) ) == 0 )
	{
		end_op();

		return - 1;  // oldpath does not exist
	}

	ilock( ip );

	if ( ip->type == T_DIR )
	{
		iunlockput( ip );

		end_op();

		return - 1;  // oldpath is a directory
	}

	// Increment the inode's link count
	ip->nlink += 1;

	iupdate( ip );  // write changes to disk

	iunlock( ip );


	// Create a new directory entry for it in directory newpath
	if ( ( dir = nameiparent( newpath, name ) ) == 0 )
	{
		goto bad;  // newpath does not exist
	}

	ilock( dir );

	if ( dir->dev != ip->dev || dirlink( dir, name, ip->inum ) < 0 )
	{
		/* a) newpath is not on the same device (an error since
		      inode numbers only have a unique meaning on a single disk)
		   b) dirlink failed
		*/
		iunlockput( dir );

		goto bad;
	}

	iunlockput( dir );

	iput( ip );

	end_op();

	return 0;

bad:

	ilock( ip );

	ip->nlink -= 1;  // decrement (undo increment)

	iupdate( ip );

	iunlockput( ip );

	end_op();

	return - 1;
}

// Is the directory empty except for "." and ".." ?
static int isdirempty ( struct inode* dir )
{
	struct dirent direntry;
	int           off;
	int           nread;

	for ( off = 2 * sizeof( direntry ); off < dir->size; off += sizeof( direntry ) )
	{
		nread = readi( dir, ( char* ) &direntry, off, sizeof( direntry ) );

		if ( nread != sizeof( direntry ) )
		{
			panic( "isdirempty: readi" );
		}

		if ( direntry.inum != 0 )
		{
			return 0;
		}
	}

	return 1;
}

// ...
int sys_unlink ( void )
{
	struct inode*  ip;
	struct inode*  parentdir;
	struct dirent  direntry;
	char           name [ FILENAMESZ ];
	char*          path;
	uint           off;
	int            nbytes;

	if ( argstr( 0, &path ) < 0 )
	{
		return - 1;
	}

	begin_op();

	if ( ( parentdir = nameiparent( path, name ) ) == 0 )
	{
		end_op();

		return - 1;
	}

	ilock( parentdir );

	// Cannot unlink "." or "..".
	if ( namecmp( name, "." ) == 0 || namecmp( name, ".." ) == 0 )
	{
		goto bad;
	}


	// Get inode associated with 'name'
	if ( ( ip = dirlookup( parentdir, name, &off ) ) == 0 )
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


	// Clear the directory entry
	memset( &direntry, 0, sizeof( direntry ) );

	nbytes = writei( parentdir, ( char* ) &direntry, off, sizeof( direntry ) );

	if ( nbytes != sizeof( direntry ) )
	{
		panic( "unlink: writei" );
	}

	if ( ip->type == T_DIR )
	{
		parentdir->nlink -= 1;

		iupdate( parentdir );
	}

	iunlockput( parentdir );


	// Update the inode's link count
	ip->nlink -= 1;

	iupdate( ip );

	iunlockput( ip );


	end_op();

	return 0;

bad:

	iunlockput( parentdir );

	end_op();

	return - 1;
}


// ___________________________________________________________________________

/* Whereas sys_link creates a new name for an existing inode,
   'create' creates a new name for a new inode...

   create is a generalization of three file creating syscalls:
 
     . open with the O_CREATE flag
       . makes an ordinary file

     . mkdir
       . makes a new directory

     . mkdev
       . makes a new device file
*/
static struct inode* create ( char* path, short type, short major, short minor )
{
	struct inode* ip;
	struct inode* parentdir;
	uint          off;
	char          name [ FILENAMESZ ];

	// Get parent directory
	if ( ( parentdir = nameiparent( path, name ) ) == 0 )
	{
		return 0;
	}

	ilock( parentdir );

	// Check if name is already present in parent directory
	if ( ( ip = dirlookup( parentdir, name, &off ) ) != 0 )
	{
		iunlockput( parentdir );

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

	// Allocate an inode
	if ( ( ip = ialloc( parentdir->dev, type ) ) == 0 )
	{
		panic( "create: ialloc" );
	}

	ilock( ip );

	ip->major = major;
	ip->minor = minor;
	ip->nlink = 1;

	iupdate( ip );


	// If we are creating a directory, add the . and .. entries
	if ( type == T_DIR )
	{
		parentdir->nlink += 1;   // for ".."

		iupdate( parentdir );


		// No ip->nlink += 1 for ".": avoid cyclic ref count.

		if ( dirlink( ip,  ".", ip->inum        ) < 0  ||  // current directory
			 dirlink( ip, "..", parentdir->inum ) < 0 )    // parent directory
		{
			panic( "create: dots" );
		}
	}


	// Add the new file to the parent directory
	if ( dirlink( parentdir, name, ip->inum ) < 0 )
	{
		panic( "create: dirlink" );
	}

	iunlockput( parentdir );

	return ip;
}


// ___________________________________________________________________________

/*
*/
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

		ilock( ip );  // unlike 'create', 'namei' does not return a locked inode

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

	//
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

	if ( argstr( 0, &path ) < 0                      ||
		 ( ip = create( path, T_DIR, 0, 0 ) ) == 0 )
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

	if ( argstr( 0, &path ) < 0        ||
		 ( ip = namei( path ) ) == 0 )
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

		// Get pointer to string argument
		if ( fetchint( uargv + ( 4 * i ), ( int* ) &uarg ) < 0 )
		{
			return - 1;
		}

		if ( uarg == 0 )
		{
			argv[ i ] = 0;

			break;
		}

		// Get the string
		if ( fetchstr( uarg, &argv[ i ] ) < 0 )
		{
			return - 1;
		}
	}

	return exec( path, argv );
}


// ___________________________________________________________________________

/* Connects pipe to file system by providing a way to
   create a pipe pair...
   Its argument is a pointer to space for two integers, where
   it will record the two new file descriptors...
   It allocates the pupe and installs the file descriptors...
*/
int sys_pipe ( void )
{
	struct file* rf;
	struct file* wf;
	int*         fd;
	int          fd0,
	             fd1;

	// Get fd arg... "pipe( fd )"
	if ( argptr( 0, ( void* ) &fd, 2 * sizeof( fd[ 0 ] ) ) < 0 )
	{
		return - 1;
	}

	/* Create a pipe.
	   Also allocate file structures (rf and wf) and set them as
	   the pipe's read and write ends...
	*/
	if ( pipealloc( &rf, &wf ) < 0 )
	{
		return - 1;
	}


	/* Allocate file descriptors (in the current process) for the
	   pipe's read and write ends
	*/
	fd0 = fdalloc( rf );

	if ( fd0 < 0 )
	{
		fileclose( rf );

		fileclose( wf );

		return - 1;
	}

	fd1 = fdalloc( wf );

	if ( fd1 < 0 )
	{
		// If fd0 was successfully allocated, deallocate it
		if ( fd0 >= 0 )
		{
			myproc()->ofile[ fd0 ] = 0;
		}

		fileclose( rf );

		fileclose( wf );

		return - 1;
	}


	/* Return the pipe's file descriptors to the caller.
	   Indirectly return by updating contents pointed to by arg 'fd'.
	*/
	fd[ 0 ] = fd0;
	fd[ 1 ] = fd1;

	return 0;
}


// ___________________________________________________________________________

/* ...
   https://github.com/DoctorWkt/xv6-freebsd/blob/master/kern/sysfile.c

   int ioctl ( int fd, int request, ... );
   int ioctl ( int fd, int request, uint* argp );  // Make life simpler...
*/
int sys_ioctl ( void )
{
	int          fd;
	int          request;
	uint*        argp;
	struct file* f;

	if ( argfd( 0, &fd, &f ) < 0   ||
		 argint( 1, &request ) < 0 ||
		 argptr( 2, ( void* ) &argp, sizeof( uint ) ) < 0 )  /* size should be "nArgs * sizeof( uint )"
		                                                        but nArgs is unknown. This introduces
		                                                        opportunity for exploit since boundary
		                                                        check in argptr is bypassed. */
	{
		return - 1;
	}

	if ( ( f->type != FD_INODE ) || ( f->ip->type != T_DEV ) )
	{
		return - 1;
	}

	if ( f->ip->major < 0 || f->ip->major >= NDEV || ! devsw[ f->ip->major ].ioctl )
	{
		return - 1;
	}

	return devsw[ f->ip->major ].ioctl( f->ip, request, argp );
}
