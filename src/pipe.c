#include "types.h"
#include "defs.h"
#include "param.h"
#include "mmu.h"
#include "proc.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

#define PIPESIZE 512

struct pipe {

	/* This lock is used for ... ??
	*/
	struct spinlock lock;

	/* The buffer wraps around (is circular).
	   The next byte written after [PIPESIZE - 1] is [0].
	   The counts (nread, nwrite) however do not wrap.

	   This makes it possible to distinguish between a
	   full buffer ( nwrite == nread + PIPESIZE ) and an
	   empty one ( nwrite == nread )
	*/
	char data [ PIPESIZE ];  // buffer
	uint nread;              // number of bytes read from the buffer
	uint nwrite;             // number of bytes written to the buffer

	int  readopen;           // read fd is still open
	int  writeopen;          // write fd is still open
};

int pipealloc ( struct file** f0, struct file** f1 )
{
	struct pipe* p;

	p   = 0;
	*f0 = 0;
	*f1 = 0;

	if ( ( *f0 = filealloc() ) == 0 || ( *f1 = filealloc() ) == 0 )
	{
		goto bad;
	}

	if ( ( p = ( struct pipe* ) kalloc() ) == 0 )
	{
		goto bad;
	}

	p->readopen  = 1;
	p->writeopen = 1;
	p->nwrite    = 0;
	p->nread     = 0;

	initlock( &p->lock, "pipe" );

	( *f0 )->type     = FD_PIPE;
	( *f0 )->readable = 1;
	( *f0 )->writable = 0;
	( *f0 )->pipe     = p;

	( *f1 )->type     = FD_PIPE;
	( *f1 )->readable = 0;
	( *f1 )->writable = 1;
	( *f1 )->pipe     = p;

	return 0;

bad:

	if ( p )
	{
		kfree( ( char* ) p );
	}

	if ( *f0 )
	{
		fileclose( *f0 );
	}

	if ( *f1 )
	{
		fileclose( *f1 );
	}

	return - 1;
}

void pipeclose ( struct pipe* p, int writable )
{
	acquire( &p->lock );

	if ( writable )
	{
		p->writeopen = 0;

		wakeup( &p->nread );
	}
	else
	{
		p->readopen = 0;

		wakeup( &p->nwrite );
	}

	if ( p->readopen == 0 && p->writeopen == 0 )
	{
		release( &p->lock );

		kfree( ( char* ) p );
	}
	else
	{
		release( &p->lock );
	}
}

int pipewrite ( struct pipe* p, char* addr, int n )
{
	int i;

	acquire( &p->lock );

	// For each byte to be written...
	for ( i = 0; i < n; i += 1 )
	{
		// If buffer is full, wait for bytes to be read off it
		while ( p->nwrite == p->nread + PIPESIZE )
		{
			// ...
			if ( p->readopen == 0 || myproc()->killed )
			{
				release( &p->lock );

				return - 1;
			}

			// Alert any sleeping readers that there is data waiting to be read
			wakeup( &p->nread );

			// Sleep, waiting for a reader to take some bytes off the buffer
			sleep( &p->nwrite, &p->lock );
		}


		// Otherwise, write byte to buffer
		p->data[ p->nwrite % PIPESIZE ] = addr[ i ];

		p->nwrite += 1;
	}


	// Alert any sleeping readers that there is data waiting to be read
	wakeup( &p->nread );

	release( &p->lock );

	return n;
}

/* JK - Why does read not read all requested bytes? Is it
   to deal with EOF?
*/
int piperead ( struct pipe* p, char* addr, int n )
{
	int i;

	acquire( &p->lock );

	// If buffer is empty, wait for bytes to be written to it
	while ( p->nread == p->nwrite )
	{
		// ...
		if ( p->writeopen == 0 || myproc()->killed )
		{
			release( &p->lock );

			return - 1;
		}

		// Sleep, waiting for a writer to write some bytes onto the buffer
		sleep( &p->nread, &p->lock );
	}


	// Otherwise, read bytes from buffer
	for ( i = 0; i < n; i += 1 )
	{
		// If buffer is empty...
		if ( p->nread == p->nwrite )
		{
			break;
		}

		addr[ i ] = p->data[ p->nread % PIPESIZE ];

		p->nread += 1;
	}


	// Alert any sleeping writers that some bytes in the buffer have been freed
	wakeup( &p->nwrite );

	release( &p->lock );

	return i;  // number of bytes read may be less than requested
}
