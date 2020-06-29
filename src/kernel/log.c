// Simple logging that allows concurrent FS system calls.
//
// A log transaction contains the updates of multiple FS system
// calls. The logging system only commits when there are
// no FS system calls active. Thus there is never
// any reasoning required about whether a commit might
// write an uncommitted system call's updates to disk.
//
// A system call should call begin_op()/end_op() to mark
// its start and end. Usually begin_op() just increments
// the count of in-progress FS system calls and returns.
// But if it thinks the log is close to running out, it
// sleeps until the last outstanding end_op() commits.
//
// The log is a physical re-do log containing disk blocks.
// The on-disk log format:
//   header block, containing block #s for block A, B, C, ...
//   block A
//   block B
//   block C
//   ...
// Log appends are synchronous.

/* Because many fs operations involve multiple writes to the disk,
   a crash after a subset of the writes might leave the on-disk fs
   in an inconsitenent state.

   To prevent this, an xv6 system call does not directly write to
   the on-disk fs. Instead,
     . It place a description of all the disk writes it wish to make
       in a log on the disk.
     . Once the syscall has logged all of its writes, it writes a
       special "commit" record to the log indicating that the log
       contains a complete operation.
     . At this point, the syscall then copies the writes to the on-disk fs ??
     . After the writes are completed, the syscall erases the on-disk log ??

   Should the system crash and reboot, the fs recovers from the crash
   as follows before running any process:
     . If the log is marked as containing a complete operation, the
       recovery code copies the writes to where they belong in the on-disk fs
     . If the log does not contain a complete operation, the recovery
       code ignores the log
     . The recovery code finshes by erasing the log...

   The log makes operations atomic with respect to crashes: after
   recovery, either all of the operation's writes will appear on
   the disk, or none of them.

   The log resides in the disk at a known fixed location (superblock->logstart).
   It consists of a header block followed by a sequence of updated
   block copies ("logged blocks").

   The header block contains:
     . an array of sector numbers, one for each loggged block
     . a count of the number of logged blocks

   Diagram of the log: 

     on-disk
       | logheader | logblock | ... | logblock |

       . because of 'bwrite' (more specifically 'len( buf->data ) == BLOCKSIZE'),
         logheader takes up a full block on disk

     in-memory ("struct log")
       | spinlock | start | size | dev | outstanding | committing | logheader |


   Interface:
     . begin_op / end_op : Indicate start of sequence of writes that must be
                           atomic with respect to crashes
     . log_write         : Acts as a proxy for bwrite

   Typical use looks like:

       begin_op();

       ...

       buf = bread( ... );

       buf->data[ ... ] = ...;

       log_write( buf );

       ...

       end_op();


   To allow concurrent execution of fs operations, the logging system can
   accumulate the writes of multiple processes into one transaction
   (log->outstanding += 1)

   To avoid splitting an fs syscall across transactions, the log only
   commits when no fs syscalls are underway (log->outstanding == 0)


   xv6 dedicates a fixed amount of space on the disk to hold the log.
   As such, no single syscall can be allowed to write more distinct blocks
   than there is space in the log.
   The syscalls 'write' and 'unlink'(?) can potentially write many blocks
   (when handling large files...).
   To address this, the 'write' syscall breaks up large writes into
   multiple smaller writes.


   xv6's logging system is inefficient:
     . a commit cannot occur concurrently with fs syscalls
     . an entire block is logged even if only a few bytes are changed
     . log writes are synchronous
*/

#include "types.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "date.h"
#include "fs.h"
#include "buf.h"

/* Used by both the on-disk and in-memory log headers
   to keep track of logged blocks
*/
struct logheader
{
	int n;                      // count of logged blocks
	int blocklist [ LOGSIZE ];  // array of block numbers, one for each logged block...
};

// Holds one transaction...
struct log
{
	/* This lock is used for ... ??
	*/
	struct spinlock  lock;

	int              start;
	int              size;
	int              dev;

	int              outstanding;  // how many FS syscalls are executing.
	int              committing;   // in commit(), please wait.

	struct logheader header;       // in-memory log header
};
struct log log;


static void recover_from_log ( void );


//
void initlog ( int dev )
{
	if ( sizeof( struct logheader ) >= BLOCKSIZE )
	{
		panic( "initlog: too big logheader" );
	}

	struct superblock sb;

	initlock( &log.lock, "log" );

	readsb( dev, &sb );

	log.start = sb.logstart;
	log.size  = sb.nlogblocks;
	log.dev   = dev;

	recover_from_log();
}


// __________________________________________________________________________________

// Read the log header from disk into the in-memory log header
static void read_disk_logheader ( void )
{
	struct buf*       buffer;
	struct logheader* header_disk;
	int               i;

	buffer = bread( log.dev, log.start );

	header_disk = ( struct logheader* ) ( buffer->data );

	log.header.n = header_disk->n;

	for ( i = 0; i < log.header.n; i += 1 )
	{
		log.header.blocklist[ i ] = header_disk->blocklist[ i ];
	}

	brelse( buffer );
}

// Write the in-memory log header to the on-disk log header.
// This is the true point at which the current transaction commits.
static void write_disk_logheader ( void )
{
	struct buf*       buffer;
	struct logheader* header_disk;
	int               i;

	buffer = bread( log.dev, log.start );

	header_disk = ( struct logheader* ) ( buffer->data );

	header_disk->n = log.header.n;

	for ( i = 0; i < log.header.n; i += 1 )
	{
		header_disk->blocklist[ i ] = log.header.blocklist[ i ];
	}

	bwrite( buffer );  // write changes to disk

	brelse( buffer );
}


// __________________________________________________________________________________

// Copy modified blocks from the buffer cache to the on-disk log.
/* Copies each block modified in the transaction from the
   buffer cache to its slot in the on-disk log
*/
static void write_disk_logblocks ( void )
{
	struct buf* cache;
	struct buf* disklog;
	int         idx;

	for ( idx = 0; idx < log.header.n; idx += 1 )
	{
		cache   = bread( log.dev, log.header.blocklist[ idx ] );  // block in buffer cache

		disklog = bread( log.dev, log.start + idx + 1 );      // block in on-disk log

		memmove( disklog->data, cache->data, BLOCKSIZE );  // memove( dst, src, nbytes )

		bwrite( disklog );  // write changes to disk

		brelse( cache );
		brelse( disklog );
	}
}

// Copy committed blocks from the on-disk log to their on-disk fs location
static void install_transaction ( void )
{
	struct buf* disklog;
	struct buf* diskfs;
	int         idx;

	for ( idx = 0; idx < log.header.n; idx += 1 )
	{
		disklog = bread( log.dev, log.start + idx + 1 );      // block in on-disk log

		diskfs  = bread( log.dev, log.header.blocklist[ idx ] );  // block in on-disk fs

		memmove( diskfs->data, disklog->data, BLOCKSIZE );  // memove( dst, src, nbytes )

		bwrite( diskfs );  // write changes to disk

		brelse( disklog );
		brelse( diskfs );
	}
}

//
/* xv6 writes the header block only when a transaction commits.
   It sets the count to zero after copying the logged blocks
   to the fs.
*/
static void commit ( void )
{
	if ( log.header.n > 0 )
	{
		// Copy modified blocks from buffer cache to on-disk log
		write_disk_logblocks();


		// Write in-memory header to on-disk header
		/* This is the *real* commit point.
		   On recovery, this is now the logheader that will be read
		   from disk and used to replay writes...
		*/
		write_disk_logheader();  // after this, the on-disk header.n > 0


		// Install writes to on-disk fs
		install_transaction();


		// Erase the transaction from the log
		log.header.n = 0;

		write_disk_logheader();  // after this, the on-disk header.n == 0
	}
}


// __________________________________________________________________________________

// Recover a transaction interrupted by a crash
static void recover_from_log ( void )
{
	// Read on-disk header into in-memory header
	read_disk_logheader();


	// If committed (log.header.n > 0), copy from on-disk log to on-disk fs
	install_transaction();


	// Erase the transaction from the log
	log.header.n = 0;

	write_disk_logheader();
}


// __________________________________________________________________________________

// Called at the start of each FS system call
/* Waits until the logging system is not currently committing, and
   until there is enough free log space to hold the syscall and all
   currently executing fs syscalls

   log.outstanding counts the number of FS syscalls that are currently executing.
   Incrementing its value both reserves space ?? and prevents a commit
   from occuring during ??

   We conservatively assume that each syscall might write up to
   MAXOPBLOCKS distinct blocks...
*/
void begin_op ( void )
{
	acquire( &log.lock );

	while ( 1 )
	{
		// Wait until logging system is not currently committing
		if ( log.committing )
		{
			sleep( &log, &log.lock );
		}
		// Wait until there is enough free log space
		else if ( log.header.n + ( log.outstanding + 1 ) * MAXOPBLOCKS > LOGSIZE )
		{
			sleep( &log, &log.lock );
		}
		// Increment log.outstanding
		else
		{
			log.outstanding += 1;

			release( &log.lock );

			break;
		}
	}
}

// Called at the end of each FS system call.
// Commits if this was the last outstanding operation.
/* Decrements log.outstanding.
   If the count is now zero, it commits the current transaction.
*/
void end_op ( void )
{
	int do_commit = 0;

	acquire( &log.lock );


	log.outstanding -= 1;  // decrement count


	if ( log.committing )
	{
		panic( "end_op: log.committing" );
	}


	// If the count is now zero, plan to commit
	if ( log.outstanding == 0 )
	{
		do_commit = 1;

		log.committing = 1;
	}
	else
	{
		/* begin_op() may be waiting for log space,
		   and decrementing log.outstanding has decreased
		   the amount of reserved space.
		*/
		wakeup( &log );
	}

	release( &log.lock );


	// Make the commit
	if ( do_commit )
	{
		/* Call commit without holding locks, since not allowed
		   to sleep with locks...
		*/
		commit();


		acquire( &log.lock );

		log.committing = 0;

		// begin_op() may be waiting for log.committing to equal 0
		wakeup( &log );

		release( &log.lock );
	}
}


// __________________________________________________________________________________

// Add the buffer to the in-memory log's block list ...
//
/* Caller has modified b->data and is done with the buffer.
   Record the block number and pin in the cache with B_DIRTY.
   commit > write_disk_logblocks will do the disk write.
  
   log_write() replaces bwrite(); a typical use is:
     bp = bread( ... )
     modify bp->data[]
     log_write( bp )
     brelse( bp )
*/
/* Acts as a proxy for bwrite.

   It records the block's sector number in memory, reserving it
   a slot in the on-disk log ??

   It also marks the buffer as dirty to prevent the buffer cache
   from evicting it...
   The block must stay in the cache until its committed to disk.
   Until then, the cached copy is the only record of the modification.

   log_write notices when a block is written multiple times during a
   single transaction, and allocates the block the same slot in the log.
   By absorbing several disk writes into one, the fs can:
     . save log space
     . achieve better performance, because only one copy of the block
       is written to disk...
*/
void log_write ( struct buf* b )
{
	int i;

	if ( log.header.n >= LOGSIZE || log.header.n >= log.size - 1 )
	{
		panic( "log_write: too big a transaction" );
	}

	if ( log.outstanding < 1 )
	{
		panic( "log_write: outside of trans" );
	}

	acquire( &log.lock );

	// Log absorption
	for ( i = 0; i < log.header.n; i += 1 )
	{
		if ( log.header.blocklist[ i ] == b->blockno )
		{
			break;
		}
	}

	log.header.blocklist[ i ] = b->blockno;

	if ( i == log.header.n )
	{
		log.header.n += 1;  // only if unique blockno
	}

	b->flags |= B_DIRTY;  // prevent eviction from cache
	release( &log.lock );
}
