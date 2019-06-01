#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <dirent.h>     // addDirectory
#include <sys/types.h>  // addDirectory
#include <sys/stat.h>   // addDirectory

#define stat   xv6_stat    // avoid clash with host struct stat
#define dirent xv6_dirent  // avoid clash with host struct dirent

	#include "types.h"
	#include "fs.h"
	#include "stat.h"
	#include "param.h"

#undef stat
#undef dirent

#ifndef static_assert
	#define static_assert( a, b ) do { switch ( 0 ) case 0: case ( a ): ; } while ( 0 )
#endif

#define NINODES 200

// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]

int nbitmap      = FSSIZE / ( BSIZE * 8 ) + 1;
int ninodeblocks = ( NINODES / IPB ) + 1;
int nlog         = LOGSIZE;
int nmeta;     // Number of meta blocks ( boot, sb, nlog, inode, bitmap )
int nblocks;   // Number of data blocks

int               fsfd;
struct superblock sb;
char              zeroes [ BSIZE ];
uint              freeinode = 1;
uint              freeblock;


void balloc  ( int );
void wsect   ( uint, void* );
void winode  ( uint, struct dinode* );
void rinode  ( uint inum, struct dinode *ip );
void rsect   ( uint sec, void *buf );
uint ialloc  ( ushort type );
void iappend ( uint inum, void *p, int n );

void addDirectoryEntry ( int, char*, int );
void addFile           ( int, char* );
void addDirectory      ( int, char* );
int  makeDirectory     ( int, char* );


// ___________________________________________________________________

// convert to intel byte order - why ??
ushort xshort ( ushort x )
{
	ushort y;

	uchar *a = ( uchar* )&y;

	a[ 0 ] = x;
	a[ 1 ] = x >> 8;

	return y;
}

uint xint ( uint x )
{
	uint y;

	uchar *a = ( uchar* )&y;

	a[ 0 ] = x;
	a[ 1 ] = x >> 8;
	a[ 2 ] = x >> 16;
	a[ 3 ] = x >> 24;

	return y;
}


// ___________________________________________________________________

int main ( int argc, char *argv[] )
{
	int           i;
	uint          rootino,
	              off;
	char          buf [ BSIZE ];
	struct dinode din;


	static_assert( sizeof( int ) == 4, "Integers must be 4 bytes!" );

	if ( argc < 2 )
	{
		fprintf( stderr, "Usage: mkfs fs.img files...\n" );

		exit( 1 );
	}

	assert( ( BSIZE % sizeof( struct dinode ) ) == 0 );
	assert( ( BSIZE % sizeof( struct xv6_dirent ) ) == 0 );

	// Open fs.img
	fsfd = open( argv[ 1 ], O_RDWR | O_CREAT | O_TRUNC, 0666 );

	if ( fsfd < 0 )
	{
		perror( argv[ 1 ] );

		exit( 1 );
	}

	// 1 fs block == 1 disk sector
	nmeta = 2 + nlog + ninodeblocks + nbitmap;

	nblocks = FSSIZE - nmeta;

	sb.size       = xint( FSSIZE );
	sb.nblocks    = xint( nblocks );
	sb.ninodes    = xint( NINODES );  // why not ninodeblocks ??
	sb.nlog       = xint( nlog );
	sb.logstart   = xint( 2 );
	sb.inodestart = xint( 2 + nlog );
	sb.bmapstart  = xint( 2 + nlog + ninodeblocks );

	printf(

		"meta blocks  %d\n"
		"    boot          1\n"
		"    super         1\n"
		"    log blocks    %u\n"
		"    inode blocks  %u\n"
		"    bitmap blocks %u\n"
		"data blocks  %d\n"
		"total blocks %d\n\n",

		nmeta, nlog, ninodeblocks, nbitmap,
		nblocks, FSSIZE 
	);

	// the first free block that we can allocate
	freeblock = nmeta;

	// Write zeroes to entire fs ??
	for ( i = 0; i < FSSIZE; i += 1 )
	{
		wsect( i, zeroes );
	}

	// Write super block
	memset( buf, 0, sizeof( buf ) );

	memmove( buf, &sb, sizeof( sb ) );

	wsect( 1, buf );


	// Create root inode
	rootino = ialloc( T_DIR );

	assert( rootino == ROOTINO );


	// Create "." directory entry for root
	addDirectoryEntry( rootino, ".", rootino );

	// Create ".." directory entry for root
	addDirectoryEntry( rootino, "..", rootino );


	// Clone an exisiting directory
	addDirectory( rootino, argv[ 2 ] );


	// Fix size of root inode dir ??
	rinode( rootino, &din );

	off = xint( din.size );
	off = ( ( off / BSIZE ) + 1 ) * BSIZE;

	din.size = xint( off );

	winode( rootino, &din );


	// Create bitmap
	balloc( freeblock );

	exit( 0 );
}


// ___________________________________________________________________

/* Clone an exisiting directory.
   Code by Warren Toomey,
    https://github.com/DoctorWkt/xv6-freebsd/blob/master/Makefile
    https://github.com/DoctorWkt/xv6-freebsd/blob/master/tools/mkfs.c
*/

// Add the given filename and i-number as a directory entry
void addDirectoryEntry ( int dirino, char *name, int fileino )
{
	struct xv6_dirent de;

	bzero( &de, sizeof( de ) );

	de.inum = xshort( fileino );

	strncpy( de.name, name, DIRNAMESZ );

	iappend( dirino, &de, sizeof( de ) );
}

/* Make a new directory entry in the directory specified by
   the given i-number. Return the new directory's i-number
*/
int makeDirectory ( int parentdirino, char *newdirname )
{
	int newdirino;

	// Allocate an inode number for the directory
	newdirino = ialloc( T_DIR );

	// Set up the "." and ".." entries
	addDirectoryEntry( newdirino, ".",  newdirino );     // self
	addDirectoryEntry( newdirino, "..", parentdirino );  // parent

	// Add the new directory to the parent directory
	addDirectoryEntry( parentdirino, newdirname, newdirino );

	return newdirino;
}

// Add a file to the directory specified by the given i-number
void addFile ( int dirino, char *filename )
{
	char buf [ BSIZE ];
	int  cc,
	     fd,
	     inum;

	// Open the file
	if ( ( fd = open( filename, 0 ) ) < 0 )
	{
		perror( "open" );

		exit( 1 );
	}

	// Create an i-node for the file
	inum = ialloc( T_FILE );

	// Create a directory entry for the file
	addDirectoryEntry( dirino, filename, inum );

	// Write the file's contents to the inode's data blocks
	while ( ( cc = read( fd, buf, sizeof( buf ) ) ) > 0 )
	{
		iappend( inum, buf, cc );
	}

	close( fd );
}

/* Given a local directory name and a directory i-number
   on the image, add all the files from the local directory
   to the on-image directory.
*/
void addDirectory ( int dirino, char *localdirname )
{
	DIR           *dir;
	struct dirent *dent;
	struct stat    st;
	int            newdirino;

	dir = opendir( localdirname );

	if ( dir == NULL )
	{
		perror( "opendir" );

		exit( 1 );
	}

	chdir( localdirname );

	while ( ( dent = readdir( dir ) ) != NULL )
	{
		// Skip the "." entry
		if ( ! strcmp( dent->d_name, "." ) )
		{
			continue;
		}

		// Skip the ".." entry
		if ( ! strcmp( dent->d_name, ".." ) )
		{
			continue;
		}

		// Get information about the file
		if ( stat( dent->d_name, &st ) == - 1 )
		{
			perror( "stat" );

			exit( 1 );
		}

		// File is a directory
		if ( S_ISDIR( st.st_mode ) )
		{
			newdirino = makeDirectory( dirino, dent->d_name );

			addDirectory( newdirino, dent->d_name );
		}
		// File is a regular file
		else if ( S_ISREG( st.st_mode ) )
		{
			addFile( dirino, dent->d_name );
		}
	}

	closedir( dir );

	chdir( ".." );  // go back up to parent?
}


// ___________________________________________________________________

void wsect ( uint sec, void *buf )
{
	if ( lseek( fsfd, sec * BSIZE, 0 ) != sec * BSIZE )
	{
		perror( "lseek" );

		exit( 1 );
	}

	if ( write( fsfd, buf, BSIZE ) != BSIZE )
	{
		perror( "write" );

		exit( 1 );
	}
}

void rsect ( uint sec, void *buf )
{
	if ( lseek( fsfd, sec * BSIZE, 0 ) != sec * BSIZE )
	{
		perror( "lseek" );

		exit( 1 );
	}
	if ( read( fsfd, buf, BSIZE ) != BSIZE )
	{
		perror( "read" );

		exit( 1 );
	}
}

void winode ( uint inum, struct dinode *ip )
{
	char           buf [ BSIZE ];
	uint           bn;
	struct dinode *dip;

	bn = IBLOCK( inum, sb );

	rsect( bn, buf );

	dip = ( ( struct dinode* )buf ) + ( inum % IPB );

	*dip = *ip;

	wsect( bn, buf );
}

void rinode ( uint inum, struct dinode *ip )
{
	char           buf [ BSIZE ];
	uint           bn;
	struct dinode *dip;

	bn = IBLOCK( inum, sb );

	rsect( bn, buf );

	dip = ( ( struct dinode* )buf ) + ( inum % IPB );

	*ip = *dip;
}


// ___________________________________________________________________

uint ialloc ( ushort type )
{
	struct dinode din;
	uint          inum;

	inum = freeinode;

	freeinode += 1;

	bzero( &din, sizeof( din ) );

	din.type  = xshort( type );
	din.nlink = xshort( 1 );
	din.size  = xint( 0 );

	winode( inum, &din );

	return inum;
}

void balloc ( int used )
{
	uchar buf [ BSIZE ];
	int   i;

	printf( "balloc: first %d blocks have been allocated\n", used );

	assert( used < BSIZE * 8 );

	bzero( buf, BSIZE );  // clear all bits

	for ( i = 0; i < used; i += 1 )
	{
		buf[ i / 8 ] = buf[ i / 8 ] | ( 1 << ( i % 8 ) );  // if used, set bit
	}

	printf( "balloc: write bitmap block at sector %d\n", sb.bmapstart );

	wsect( sb.bmapstart, buf );
}

#define min( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

void iappend ( uint inum, void *xp, int n )
{
	char *p = ( char* )xp;
	uint  fbn,
	      off,
	      n1;

	struct dinode  din;
	char           buf [ BSIZE ];
	uint           indirect [ NINDIRECT ];
	uint           x;

	rinode( inum, &din );

	off = xint( din.size );

	// printf( "append inum %d at off %d sz %d\n", inum, off, n );

	while ( n > 0 )
	{
		fbn = off / BSIZE;

		assert( fbn < MAXFILE );

		// Direct blocks
		if ( fbn < NDIRECT )
		{
			if ( xint( din.addrs[ fbn ] ) == 0 )
			{
				din.addrs[ fbn ] = xint( freeblock );

				freeblock += 1;
			}

			x = xint( din.addrs[ fbn ] );
		}
		// Indirect blocks
		else
		{
			// Indirect block
			if ( xint( din.addrs[ NDIRECT ] ) == 0 )
			{
				din.addrs[ NDIRECT ] = xint( freeblock );

				freeblock += 1;
			}

			// Block pointed to by indirect block
			rsect( xint( din.addrs[ NDIRECT ] ), ( char* )indirect );

			if ( indirect[ fbn - NDIRECT ] == 0 )
			{
				indirect[ fbn - NDIRECT ] = xint( freeblock );

				freeblock += 1;

				wsect( xint( din.addrs[ NDIRECT ] ), ( char* )indirect );
			}

			x = xint( indirect[fbn - NDIRECT] );
		}

		n1 = min( n, ( fbn + 1 ) * BSIZE - off );

		rsect( x, buf );

		bcopy( p, buf + off - ( fbn * BSIZE ), n1 );

		wsect( x, buf );

		n   -= n1;
		off += n1;
		p   += n1;
	}

	din.size = xint( off );

	winode( inum, &din );
}
