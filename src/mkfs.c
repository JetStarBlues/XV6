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
	#include "date.h"
	#include "fs.h"
	#include "stat.h"
	#include "param.h"

#undef stat
#undef dirent

#ifndef static_assert
	#define static_assert( a, b ) do { switch ( 0 ) case 0: case ( a ): ; } while ( 0 )
#endif


// Disk layout:
// [ boot block | sb block | log | inode blocks | free bit map | data blocks ]
// 1 fs block equals 1 disk sector

int nlogblocks,     // Number of log blocks 
    ninodeblocks,   // Number of inode blocks 
    nbitmapblocks,  // Number of bitmap blocks 
    nmeta,          // Number of meta blocks ( boot, sb, log, inode, bitmap )
    ndatablocks;    // Number of data blocks

int               fsfd;
struct superblock sb;
char              zeroes [ BLOCKSIZE ];
uint              freeinode;  // max can be is ?? FSNINODE
uint              freeblock;  // max can be is ?? FSSIZE


void balloc  ( int );
void wsect   ( uint, void* );
void winode  ( uint, struct dinode* );
void rinode  ( uint inum, struct dinode* ip );
void rsect   ( uint sec, void* buf );
uint ialloc  ( ushort type );
void iappend ( uint inum, void* p, int n );

void addDirectoryEntry ( int, char*, int );
void addFile           ( int, char* );
void addDirectory      ( int, char* );
int  makeDirectory     ( int, char* );


// ___________________________________________________________________

// convert to intel byte order - why ??
ushort xshort ( ushort x )
{
	ushort y;

	uchar* a = ( uchar* ) &y;

	a[ 0 ] = x;
	a[ 1 ] = x >> 8;

	return y;
}

uint xint ( uint x )
{
	uint y;

	uchar* a = ( uchar* ) &y;

	a[ 0 ] = x;
	a[ 1 ] = x >> 8;
	a[ 2 ] = x >> 16;
	a[ 3 ] = x >> 24;

	return y;
}


// ___________________________________________________________________

int main ( int argc, char* argv [] )
{
	int           i;
	uint          root_inum,
	              off;
	char          buf [ BLOCKSIZE ];
	struct dinode din;


	if ( argc < 2 )
	{
		fprintf( stderr, "Usage: mkfs fs.img files...\n" );

		exit( 1 );
	}

	static_assert( sizeof( int ) == 4, "Integers must be 4 bytes!" );

	assert( ( BLOCKSIZE % sizeof( struct dinode ) ) == 0 );
	assert( ( BLOCKSIZE % sizeof( struct xv6_dirent ) ) == 0 );


	// Open fs.img
	fsfd = open( argv[ 1 ], O_RDWR | O_CREAT | O_TRUNC, 0666 );

	if ( fsfd < 0 )
	{
		perror( argv[ 1 ] );

		exit( 1 );
	}


	// Prepare superblock
	nbitmapblocks = FSSIZE / ( BLOCKSIZE * 8 ) + 1;
	ninodeblocks  = ( FSNINODE / INODES_PER_BLOCK ) + 1;
	nlogblocks    = LOGSIZE;

	nmeta = 2 + nlogblocks + ninodeblocks + nbitmapblocks;

	ndatablocks = FSSIZE - nmeta;

	sb.size        = xint( FSSIZE );
	sb.ndatablocks = xint( ndatablocks );
	sb.ninodes     = xint( FSNINODE );
	sb.nlogblocks  = xint( nlogblocks );
	sb.logstart    = xint( 2 );
	sb.inodestart  = xint( 2 + nlogblocks );
	sb.bmapstart   = xint( 2 + nlogblocks + ninodeblocks );

	printf(

		"meta blocks  %d\n"
		"    boot          1\n"
		"    super         1\n"
		"    log blocks    %u\n"
		"    inode blocks  %u\n"
		"    bitmap blocks %u\n"
		"data blocks  %d\n"
		"total blocks %d\n\n",

		nmeta, nlogblocks, ninodeblocks, nbitmapblocks,
		ndatablocks, FSSIZE 
	);

	// the first free block that we can allocate
	freeblock = nmeta;

	// Start value for inode numbers
	//  0 - reserved for null inode pointer
	//  1 - root directory
	freeinode = 1;


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
	root_inum = ialloc( T_DIR );

	assert( root_inum == ROOTINUM );


	// Create "." directory entry for root
	addDirectoryEntry( root_inum, ".", root_inum );

	// Create ".." directory entry for root
	addDirectoryEntry( root_inum, "..", root_inum );


	// Clone an exisiting directory
	addDirectory( root_inum, argv[ 2 ] );


	// Fix size of root inode dir ??
	rinode( root_inum, &din );

	off = xint( din.size );
	off = ( ( off / BLOCKSIZE ) + 1 ) * BLOCKSIZE;

	din.size = xint( off );

	winode( root_inum, &din );


	// Create bitmap
	balloc( freeblock );

	//
	exit( 0 );
}


// ___________________________________________________________________

/* Clone an exisiting directory.
   Code by Warren Toomey,
    https://github.com/DoctorWkt/xv6-freebsd/blob/master/Makefile
    https://github.com/DoctorWkt/xv6-freebsd/blob/master/tools/mkfs.c
*/

// Add the given filename and i-number as a directory entry
void addDirectoryEntry ( int dir_inum, char* name, int file_inum )
{
	struct xv6_dirent de;

	bzero( &de, sizeof( de ) );

	de.inum = xshort( file_inum );

	strncpy( de.name, name, FILENAMESZ );

	iappend( dir_inum, &de, sizeof( de ) );
}

/* Make a new directory entry in the directory specified by
   the given i-number. Return the new directory's i-number
*/
int makeDirectory ( int parentdir_inum, char* newdir_name )
{
	int newdir_inum;

	// Allocate an inode number for the directory
	newdir_inum = ialloc( T_DIR );

	// Set up the "." and ".." entries
	addDirectoryEntry( newdir_inum, ".",  newdir_inum );     // self
	addDirectoryEntry( newdir_inum, "..", parentdir_inum );  // parent

	// Add the new directory to the parent directory
	addDirectoryEntry( parentdir_inum, newdir_name, newdir_inum );

	return newdir_inum;
}

// Add a file to the directory specified by the given i-number
void addFile ( int dir_inum, char* filename )
{
	char buf [ BLOCKSIZE ];
	int  cc,
	     fd,
	     file_inum;

	// Open the file
	fd = open( filename, 0 );

	if ( fd < 0 )
	{
		perror( "open" );

		exit( 1 );
	}

	// Create an i-node for the file
	file_inum = ialloc( T_FILE );

	// Create a directory entry for the file
	addDirectoryEntry( dir_inum, filename, file_inum );

	// Write the file's contents to the inode's data blocks
	while ( 1 )
	{
		cc = read( fd, buf, sizeof( buf ) );

		if ( cc <= 0 )
		{
			break;
		}

		iappend( file_inum, buf, cc );
	}

	close( fd );
}

/* Given a local directory name and a directory i-number
   on the image, add all the files from the local directory
   to the on-image directory.
*/
void addDirectory ( int dir_inum, char* localdirname )
{
	DIR*           dir;
	struct dirent* dent;
	struct stat    st;
	int            newdir_inum;

	dir = opendir( localdirname );

	if ( dir == NULL )
	{
		perror( "opendir" );

		exit( 1 );
	}

	chdir( localdirname );

	while ( 1 )
	{
		dent = readdir( dir );

		if ( dent == NULL )
		{
			break;
		}

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
			newdir_inum = makeDirectory( dir_inum, dent->d_name );

			addDirectory( newdir_inum, dent->d_name );
		}
		// File is a regular file
		else if ( S_ISREG( st.st_mode ) )
		{
			addFile( dir_inum, dent->d_name );
		}
	}

	closedir( dir );

	chdir( ".." );  // go back up to parent?
}


// ___________________________________________________________________

void wsect ( uint sec, void* buf )
{
	if ( ( sec < 0 ) || ( sec >= FSSIZE ) )
	{
		fprintf( stderr, "wsect: invalid sector number - %d\n", sec );

		exit( 1 );
	}

	if ( lseek( fsfd, sec * BLOCKSIZE, 0 ) != sec * BLOCKSIZE )
	{
		perror( "lseek" );

		exit( 1 );
	}

	if ( write( fsfd, buf, BLOCKSIZE ) != BLOCKSIZE )
	{
		perror( "write" );

		exit( 1 );
	}
}

void rsect ( uint sec, void* buf )
{
	if ( ( sec < 0 ) || ( sec >= FSSIZE ) )
	{
		fprintf( stderr, "rsect: invalid sector number - %d\n", sec );

		exit( 1 );
	}

	if ( lseek( fsfd, sec * BLOCKSIZE, 0 ) != sec * BLOCKSIZE )
	{
		perror( "lseek" );

		exit( 1 );
	}

	if ( read( fsfd, buf, BLOCKSIZE ) != BLOCKSIZE )
	{
		perror( "read" );

		exit( 1 );
	}
}

void winode ( uint inum, struct dinode* ip )
{
	char           buf [ BLOCKSIZE ];
	uint           bn;
	struct dinode* dip;

	bn = IBLOCK( inum, sb );

	rsect( bn, buf );

	dip = ( ( struct dinode* ) buf ) + ( inum % INODES_PER_BLOCK );

	*dip = *ip;

	wsect( bn, buf );
}

void rinode ( uint inum, struct dinode* ip )
{
	char           buf [ BLOCKSIZE ];
	uint           bn;
	struct dinode* dip;

	bn = IBLOCK( inum, sb );

	rsect( bn, buf );

	dip = ( ( struct dinode* ) buf ) + ( inum % INODES_PER_BLOCK );

	*ip = *dip;
}


// ___________________________________________________________________

uint ialloc ( ushort type )
{
	struct dinode din;
	uint          inum;

	if ( freeinode >= FSNINODE )
	{
		fprintf( stderr, "ialloc: no free inodes!\n" );

		exit( 1 );
	}

	// printf( "freeinode %d\n", freeinode );

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
	uchar buf [ BLOCKSIZE ];
	int   i;

	printf( "balloc: first %d blocks have been allocated\n", used );

	assert( used < BLOCKSIZE * 8 );

	bzero( buf, BLOCKSIZE );  // clear all bits

	for ( i = 0; i < used; i += 1 )
	{
		buf[ i / 8 ] = buf[ i / 8 ] | ( 1 << ( i % 8 ) );  // if used, set bit
	}

	wsect( sb.bmapstart, buf );

	printf( "balloc: bitmap block at sector %d\n", sb.bmapstart );
}

#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

uint getfreeblock ( void )
{
	uint fb;

	if ( freeblock >= FSSIZE )
	{
		fprintf( stderr, "getfreeblock: no free blocks!\n" );

		exit( 1 );
	}

	// printf( "freeblock %d\n", freeblock );

	fb = freeblock;

	freeblock += 1;

	return fb;
}

void iappend ( uint inum, void* xp, int n )
{
	char* p;
	uint  fbn;
	uint  off;
	uint  n1;
	uint  _freeblock;

	struct dinode  din;
	char           buf      [ BLOCKSIZE ];
	uint           indirect [ NINDIRECT ];
	uint           x;

	p = ( char* ) xp;

	rinode( inum, &din );

	off = xint( din.size );

	// printf( "append inum %d at off %d sz %d\n", inum, off, n );

	while ( n > 0 )
	{
		fbn = off / BLOCKSIZE;

		assert( fbn < MAXFILESZ );

		// Direct blocks
		if ( fbn < NDIRECT )
		{
			if ( xint( din.addrs[ fbn ] ) == 0 )
			{
				_freeblock = getfreeblock();

				din.addrs[ fbn ] = xint( _freeblock );
			}

			x = xint( din.addrs[ fbn ] );
		}
		// Indirect blocks
		else
		{
			// Indirect block
			if ( xint( din.addrs[ NDIRECT ] ) == 0 )
			{
				_freeblock = getfreeblock();

				din.addrs[ NDIRECT ] = xint( _freeblock );
			}

			// Block pointed to by indirect block
			rsect( xint( din.addrs[ NDIRECT ] ), ( char* ) indirect );

			if ( indirect[ fbn - NDIRECT ] == 0 )
			{
				_freeblock = getfreeblock();

				indirect[ fbn - NDIRECT ] = xint( _freeblock );

				wsect( xint( din.addrs[ NDIRECT ] ), ( char* ) indirect );
			}

			x = xint( indirect[ fbn - NDIRECT ] );
		}

		n1 = MIN( n, ( fbn + 1 ) * BLOCKSIZE - off );

		rsect( x, buf );

		bcopy( p, buf + off - ( fbn * BLOCKSIZE ), n1 );

		wsect( x, buf );

		n   -= n1;
		off += n1;
		p   += n1;
	}

	din.size = xint( off );

	winode( inum, &din );
}
