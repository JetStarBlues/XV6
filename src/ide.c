// Simple PIO-based (non-DMA) IDE driver code.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"

#define SECTOR_SIZE   512
#define IDE_BSY       0x80
#define IDE_DRDY      0x40
#define IDE_DF        0x20
#define IDE_ERR       0x01

#define IDE_CMD_READ  0x20
#define IDE_CMD_WRITE 0x30
#define IDE_CMD_RDMUL 0xc4
#define IDE_CMD_WRMUL 0xc5

/* Queue of pending disk requests
   idequeue points to the buf now being read/written to the disk.
   idequeue->qnext points to the next buf to be processed.
*/
static struct buf* idequeue;

// Must hold idelock when modifying idequeue
static struct spinlock idelock;

//
static int havedisk1;  // Why do we care?


static void idestart ( struct buf* );

// Wait for IDE disk to become ready.
static int idewait ( int checkerr )
{
	int r;

	// Poll status bits until the busy bit (IDE_BSY) is clear
	// and the ready bit (IDE_DRDY) is set
	while ( 1 )
	{
		r = inb( 0x1f7 );

		if ( ( r & ( IDE_BSY | IDE_DRDY ) ) == IDE_DRDY )
		{
			break;
		}
	}

	if ( checkerr && ( r & ( IDE_DF | IDE_ERR ) ) != 0 )
	{
		return - 1;
	}

	return 0;
}

void ideinit ( void )
{
	int i;

	havedisk1 = 0;

	initlock( &idelock, "ide" );

	// Enable IDE interrupts on the highest numbered CPU
	ioapicenable( IRQ_IDE, ncpu - 1 );

	idewait( 0 );

	// Check if disk 1 is present
	/* Assumes disk 0 is present because the boot loader and
	   kernel were both loaded.
	*/
	outb( 0x1f6, 0xe0 | ( 1 << 4 ) );  // select disk 1

	for ( i = 0; i < 1000; i += 1 )
	{
		if ( inb( 0x1f7 ) != 0 )
		{
			havedisk1 = 1;

			break;
		}
	}

	// Switch back to disk 0.
	outb( 0x1f6, 0xe0 | ( 0 << 4 ) );  // select disk 0
}

// Start the request for b. Caller must hold idelock.
static void idestart ( struct buf* b )
{
	if ( b == 0 )
	{
		panic( "idestart" );
	}

	if ( b->blockno >= FSSIZE )
	{
		panic( "idestart: incorrect blockno" );
	}

	int sector_per_block = BLOCKSIZE / SECTOR_SIZE;

	int sector = b->blockno * sector_per_block;

	int read_cmd  = ( sector_per_block == 1 ) ? IDE_CMD_READ  : IDE_CMD_RDMUL;
	int write_cmd = ( sector_per_block == 1 ) ? IDE_CMD_WRITE : IDE_CMD_WRMUL;

	if ( sector_per_block > 7 )
	{
		panic( "idestart: sector_per_block" );
	}

	idewait( 0 );

	outb( 0x3f6, 0 );                 // generate interrupt
	outb( 0x1f2, sector_per_block );  // number of sectors
	outb( 0x1f3, sector & 0xff );
	outb( 0x1f4, ( sector >> 8 ) & 0xff );
	outb( 0x1f5, ( sector >> 16 ) & 0xff );
	outb( 0x1f6, 0xe0 | ( ( b->dev & 1 ) << 4 ) | ( ( sector >> 24 ) & 0x0f ) );


	// If the operation is a write, idestart must now supply the data
	// to the disk...
	// The interrupt will signal that the data has been written to disk
	if ( b->flags & B_DIRTY )
	{
		outb( 0x1f7, write_cmd );
		outsl( 0x1f0, b->data, BLOCKSIZE / 4 );
	}
	// If the operation is a read, the interrupt will signal that the
	// data is ready and the handler ('ideintr') will read it.
	else
	{
		outb( 0x1f7, read_cmd );
	}
}

// Interrupt handler
void ideintr ( void )
{
	struct buf* b;

	acquire( &idelock );

	// First queued buffer is the active request.
	if ( ( b = idequeue ) == 0 )
	{
		release( &idelock );

		return;
	}

	// Move next buffer to front of queue
	idequeue = b->qnext;


	// Read data if needed into buffer (request was a read)
	/* Disk controller has completed fetching the requested
	   data and is now waiting for it to be read...
	*/
	if ( ! ( b->flags & B_DIRTY ) && idewait( 1 ) >= 0 )
	{
		insl( 0x1f0, b->data, BLOCKSIZE / 4 );
	}


	// The buffer is now ready
	b->flags |= B_VALID;    // set
	b->flags &= ~ B_DIRTY;  // clear

	// Wake process waiting for this buffer
	wakeup( b );


	// Start the request of the buffer that is now at the front of the queue
	if ( idequeue != 0 )
	{
		idestart( idequeue );
	}

	release( &idelock );
}

/* Sync buf with disk.
   If B_DIRTY is set, write buf to disk, clear B_DIRTY, set B_VALID.
   Else if B_VALID is not set, read buf from disk, set B_VALID.

   Keeps a list of pending disk requests in a queue.
   Uses interrupts to find out when each request has finished ??

   It maintains the invariant that it has sent the buffer at the
   front of the queue to the disk hardware; and that the others
   are simply waiting their turn ??
*/
void iderw ( struct buf* b )
{
	struct buf** pp;

	if ( ! holdingsleep( &b->lock ) )
	{
		panic( "iderw: buf not locked" );
	}
	if ( ( b->flags & ( B_VALID | B_DIRTY ) ) == B_VALID )
	{
		panic( "iderw: nothing to do" );
	}
	if ( b->dev != 0 && ! havedisk1 )
	{
		panic( "iderw: ide disk 1 not present" );
	}

	acquire( &idelock );


	// Append buffer to the end of idequeue
	b->qnext = 0;


	// Find last element in queue...
	pp = &idequeue;

	while ( *pp != 0 )
	{
		pp = &( ( *pp )->qnext );  // TODO, break this down with temp var
	}

	*pp = b;


	// If its the only item in idequeue, start the request immediately
	if ( idequeue == b )
	{
		idestart( b );
	}


	// Wait for request to finish.
	/* Sleep, waiting for the interrupt handler ('ideintr') to
	   record in the buffer's flags that the operation is done.

	   Other processes are free to use the CPU while we sleep.
	*/
	while ( ( b->flags & ( B_VALID | B_DIRTY ) ) != B_VALID )
	{
		sleep( b, &idelock );
	}

	release( &idelock );
}
