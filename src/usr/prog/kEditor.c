/*
  Build your own text editor
  Jeremy Ruten
  https://viewsourcecode.org/snaptoken/kilo/index.html
*/

/*

TODO:
	. 

*/


#include "types.h"
#include "fcntl.h"
#include "user.h"

#define stdin  0
#define stdout 1
#define stderr 2


//
#define CTRL( k ) ( ( k ) & 0x1F )  // https://en.wikipedia.org/wiki/ASCII#Control_characters


//
struct _editorState {

	//struct termios origConsoleAttr;

	int nRows;
	int nCols;
};

static struct _editorState editorState;



//
struct charBuffer {

	char* buf;
	int   len;
};


// ____________________________________________________________________________________

void appendToBuffer ( struct charBuffer* cBuf, const char* s, int slen )
{
	char* new;

	// Grow region pointed to by cBuf.buf
	new = realloc( cBuf->buf, cBuf->len + slen );

	if ( new == NULL )
	{
		return;
	}


	// Append 's' to region
	memcpy( &( new[ cBuf->len ] ), s, slen );


	// Update
	cBuf->buf = new;
	cBuf->len += slen;
}

void freeUnderlyingBuffer ( struct charBuffer* cBuf )
{
	free( cBuf->buf );
}


// ____________________________________________________________________________________

#if 0

static struct termios origConsoleAttr;

void enableRawMode ( void )
{
	/* Using 'termios.h' protocol, would do this by
       clearing ICANON flag...

         . https://viewsourcecode.org/snaptoken/kilo/02.enteringRawMode.html

         . https://github.com/SerenityOS/serenity/blob/04b9dc2d30cfc9b383029f6a4b02e2725108b0ae/Kernel/TTY/TTY.h
         . https://github.com/SerenityOS/serenity/blob/04b9dc2d30cfc9b383029f6a4b02e2725108b0ae/Libraries/LibC/termios.h

         . https://github.com/DoctorWkt/xv6-freebsd/blob/d2a294c2a984baed27676068b15ed9a29b06ab6f/XV6_CHANGES
         . https://github.com/DoctorWkt/xv6-freebsd/blob/d2a294c2a984baed27676068b15ed9a29b06ab6f/kern/console.c
	*/

	/*
	   . clear ECHO flag
	   . clear ICANON flag
	   . clear ...
	*/

	struct termios newConsoleAttr;

	if ( getConsoleAttr( stdin, &origConsoleAttr ) < 0 )
	{
		die( "getConsoleAttr" );
	}

	memcpy( &newConsoleAttr, &origConsoleAttr, sizeof( termios ) );

	newConsoleAttr->echo   = 0;  // disable echoing
	newConsoleAttr->icanon = 0;  // disiable canonical mode input

	if ( setConsoleAttr( stdin, &newConsoleAttr ) < 0 )
	{
		die( "setConsoleAttr" );
	}
}

void disableRawMode ( void )
{
	if ( setConsoleAttr( stdin, &origConsoleAttr ) < 0 )
	{
		die( "setConsoleAttr" );
	}
}

#endif


// ____________________________________________________________________________________

void setCursorPosition ( int row, int col )
{
	// https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#reposition-the-cursor
	// write( stdout, "\x1b[H", 3 );
}

void getCursorPosition ( int* row, int* col )
{
	// *row = ...
	// *col = ...
}

void setTextColor     ( int color ) {}
void setTextBgColor   ( int color ) {}
void invertTextColors ( void )      {}


void clearScreen ( void )
{
	// https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#clear-the-screen
	// write( stdout, "\x1b[2J", 4 );
}


// ____________________________________________________________________________________

void cleanup ( void )
{
	disableRawMode();
}


void die ( char* msg )
{
	// cleanup();

	printf( stderr, "keditor: death by %s\n", msg );

	exit();
}


// ____________________________________________________________________________________

int main ( void )
{
	char c;

	//
	// enableRawMode();
	initEditor();

	while ( 1 )
	{
		// refreshScreen();
		// processKeyPress();

		if ( read( stdin, &c, 1 ) < 0 )
		{
			die( "read" );
		}

		/*if ( c == 0 )
		{
			printf( 1, "EOF\n" );
			break;
		}*/

		if ( c == CTRL( 'q' ) )
		{
			break;
		}

		printf( stdout, "(%d, %c)\n", c, c );
	}

	// cleanup();

	exit();
}




// ____________________________________________________________________________________

void initEditor ( void )
{

	// Hard code dimensions for now...
	editorState.nRows = 25;
	editorState.nCols = 80;
}


// ____________________________________________________________________________________

void refreshScreen ( void )
{
	clearScreen();
	setCursorPosition( 0, 0 );

	drawTildeRows();

	setCursorPosition( 0, 0 );
}

void drawTildeRows ( void )
{
	int y;

	for ( y = 0; y < editorState.nRows; y += 1 )
	{
		if ( y == editorState.nRows - 1 )
		{
			write( stdout, "~", 1 );  // last line. Don't want to trigger scroll...
		}
		else
		{
			write( stdout, "~\n", 2 );
		}
	}
}


