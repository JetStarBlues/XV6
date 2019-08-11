// File descriptors

/* In Unix, most resources are represented as files including
   devices (ex. console), pipes...
   The file descriptor layer is responsible for creating this uniformity.

   xv6 gives each process its own table of open files (proc->ofile).
   Each open file is represented by a "struct file".

   Each call to 'open' creats a new open file ("struct file").
   If multiple processes open the same file independently, the different
   instances will have different IO offsets (file->off).

   On the other hand, a single open file (the same "struct file") can
   appear multiple times in one process's file table and also in the
   file tables of multiple processes ... ??
   For example:
     . If a process created aliases using 'dup'
     . Via fork - parent shares open files with child. (Fork calls 'filedup')

   A reference count tracks the number of references to the
   open file (file->ref).
   A file can be open for reading, writing, or both. The readable and
   writeable fields track this (file->readable, file->writeable).

   All the open files in the system are kept in a global file table, 'ftable'.
   The file table has functions ? to:
     . allocate a file              (filealloc)
     . create a duplicate reference (filedup)
     . release a reference          (fileclose)
     . read and write data          (fileread, filewrite)
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"


// ??
struct devsw devsw [ NDEV ];

// Tracks all files open in the system...
struct {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;

	struct file     file [ NOPENFILE_SYS ];

} ftable;


void fileinit ( void )
{
	initlock( &ftable.lock, "ftable" );
}

// Allocate a file structure.
/* Scans the file table for an unreferenced file and
   returns a new reference.
*/
struct file* filealloc ( void )
{
	struct file* f;

	acquire( &ftable.lock );

	for ( f = ftable.file; f < ftable.file + NOPENFILE_SYS; f += 1 )
	{
		// Found an unreferenced file
		if ( f->ref == 0 )
		{
			f->ref = 1;

			release( &ftable.lock );

			return f;
		}
	}

	release( &ftable.lock );

	return 0;
}

// Increment ref count of file f.
struct file* filedup ( struct file* f )
{
	acquire( &ftable.lock );

	if ( f->ref < 1 )
	{
		panic( "filedup" );
	}

	f->ref += 1;

	release( &ftable.lock );

	return f;
}

// Close file f.
// Decrement ref count, close when reaches 0.
void fileclose ( struct file* f )
{
	struct file ff;

	acquire( &ftable.lock );

	if ( f->ref < 1 )
	{
		panic( "fileclose" );
	}

	f->ref -= 1;

	// Not the last reference, our job is done
	if ( f->ref > 0 )
	{
		release( &ftable.lock );

		return;
	}


	// Close the file
	ff = *f;

	f->ref  = 0;
	f->type = FD_NONE;

	release( &ftable.lock );

	// Use appropriate close implementation
	if ( ff.type == FD_PIPE )
	{
		pipeclose( ff.pipe, ff.writable );
	}
	else if ( ff.type == FD_INODE )
	{
		begin_op();

		iput( ff.ip );

		end_op();
	}
}

// Get metadata about file f.
int filestat ( struct file* f, struct stat *st )
{
	// Only allowed on inodes
	if ( f->type == FD_INODE )
	{
		ilock( f->ip );

		stati( f->ip, st );

		iunlock( f->ip );

		return 0;
	}

	return - 1;
}

// Read n bytes from file f, starting at f->off
/*
   On success, the number of bytes read is returned.
   If the number of bytes read is zero, indicates EOF.

   On error, -1 is returned (and in Linux errno is set appropriately).
*/
int fileread ( struct file* f, char* addr, int n )
{
	int nRead;

	// Check that read is allowed by the file's open mode
	if ( f->readable == 0 )
	{
		return - 1;
	}

	// Use appropriate read implementation
	if ( f->type == FD_PIPE )
	{
		return piperead( f->pipe, addr, n );
	}

	else if ( f->type == FD_INODE )
	{
		ilock( f->ip );

		if ( ( nRead = readi( f->ip, addr, f->off, n ) ) > 0 )
		{
			f->off += nRead;
		}

		iunlock( f->ip );

		return nRead;  // Might be less than n
	}

	panic( "fileread" );
}

// Write n bytes to file f, starting at f->off
int filewrite ( struct file* f, char* addr, int n )
{
	int nWritten,
	    nWrittenTotal,
	    nToWrite,
	    nYetToWrite,
	    maxCanWrite;

	// Check that write is allowed by the file's open mode
	if ( f->writable == 0 )
	{
		return - 1;
	}

	// Use appropriate write implementation
	if ( f->type == FD_PIPE )
	{
		return pipewrite( f->pipe, addr, n );
	}

	else if ( f->type == FD_INODE )
	{
		/* Breakup large writes into individual transactions of just
		   a few (#?) sectors at a time, to avoid overflowing the log
		*/
		/* Write a few blocks at a time to avoid exceeding
		   the maximum log transaction size, including:
		     . i-node (1),
		     . indirect block (1),
		     . allocation blocks (?),
		     . and 2 blocks of slop for non-aligned writes

		   Why the division by 2 ??

		   This really belongs lower down, since writei()
		   might be writing a device like the console.
		*/
		maxCanWrite = ( ( MAXOPBLOCKS - 1 - 1 - 2 ) / 2 ) * BLOCKSIZE;

		nWrittenTotal = 0;

		while ( nWrittenTotal < n )
		{
			nYetToWrite = n - nWrittenTotal;

			nToWrite = MIN( maxCanWrite, nYetToWrite );


			begin_op();

			ilock( f->ip );

			if ( ( nWritten = writei( f->ip, addr + nWrittenTotal, f->off, nToWrite ) ) > 0 )
			{
				f->off += nWritten;
			}

			iunlock( f->ip );

			end_op();


			/* writei only returns a negative on error.
			   Why do we break instead of returning an error?
			*/
			if ( nWritten < 0 )
			{
				// break;

				return - 1;  // JK
			}

			if ( nWritten != nToWrite )
			{
				panic( "filewrite: short filewrite" );
			}

			nWrittenTotal += nWritten;
		}

		if ( nWrittenTotal != n )
		{
			return - 1;
		}

		return n;
	}

	panic( "filewrite" );
}
