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
#include "user.h"
#include "GFXtext.h"  // Render graphically
// #include "termios.h"

// Temp
#define stdin  0
#define stdout 1
#define stderr 2
#define NULL   0

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
/*struct charBuffer {

	char* buf;
	int   len;
};*/


//
void die ( char* );


// ____________________________________________________________________________________

/*void appendToBuffer ( struct charBuffer* cBuf, const char* s, int slen )
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
}*/


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

	if ( getConsoleAttr( stdin, &( editorState.origConsoleAttr ) ) < 0 )
	{
		die( "getConsoleAttr" );
	}

	memcpy( &newConsoleAttr, &( editorState.origConsoleAttr ), sizeof( termios ) );

	newConsoleAttr->echo   = 0;  // disable echoing
	newConsoleAttr->icanon = 0;  // disiable canonical mode input

	if ( setConsoleAttr( stdin, &newConsoleAttr ) < 0 )
	{
		die( "setConsoleAttr" );
	}
}

void disableRawMode ( void )
{
	if ( setConsoleAttr( stdin, &( editorState.origConsoleAttr ) ) < 0 )
	{
		die( "setConsoleAttr" );
	}
}

#endif


// ____________________________________________________________________________________

/* Emulate the "Erase In Line" behaviour.

   mode 0 - erase part of line to the right of cursor (inclusive)
   mode 1 - erase part of line to the left of cursor (inclusive)
   mode 2 - erases entire line

   https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#clear-lines-one-at-a-time
*/
#define ERASELINE_RIGHT 0
#define ERASELINE_LEFT  1
#define ERASELINE_ALL   2

void eraseInLine ( uint mode )
{
	uint row;
	uint col;
	uint savedCol;

	// Invalid mode, ignore
	if ( mode > 2 )
	{
		return;
	}


	getCursorPosition( &row, &col );

	savedCol = col;  // save


	if ( mode == ERASELINE_RIGHT )
	{
		while ( col < editorState.nCols )
		{
			setCursorPosition( row, col );
			printChar( ' ' );

			col += 1;
		}
	}
	else if ( mode == ERASELINE_LEFT )
	{
		while ( col >= 0 )
		{
			setCursorPosition( row, col );
			printChar( ' ' );

			if ( col == 0 ) { break; }  // unsigned, avoid negatives...

			col -= 1;
		}
	}
	else if ( ERASELINE_ALL )
	{
		for ( col = 0; col < editorState.nCols; col += 1 )
		{
			setCursorPosition( row, col );
			printChar( ' ' );
		}
	}


	// Restore
	setCursorPosition( row, savedCol );
}

void temp_testEraseLine ( void )
{
	uint col;
	uint row;

	row = 0;

	for ( col = 0; col < editorState.nCols; col += 1 )
	{
		setCursorPosition( row, col );
		printChar( '$' );
	}

	setCursorPosition( 0, 20 );

	eraseInLine( 0 );



	row = 1;

	for ( col = 0; col < editorState.nCols; col += 1 )
	{
		setCursorPosition( row, col );
		printChar( '&' );
	}

	setCursorPosition( 1, 20 );

	eraseInLine( 1 );



	row = 2;

	for ( col = 0; col < editorState.nCols; col += 1 )
	{
		setCursorPosition( row, col );
		printChar( '#' );
	}

	setCursorPosition( 2, 20 );

	eraseInLine( 2 );
}


/* Here, since we're responsible for cursor movement...
*/
#define WRAP    0
#define NO_WRAP 1

void printString ( char* s, int wrap )
{
	uint row;
	uint col;

	getCursorPosition( &row, &col );

	while ( *s )
	{
		//
		printChar( *s );
		printf( 1, "%c", *s );


		// Advance cursor
		col += 1;

		if ( col == editorState.nCols )
		{
			if ( wrap == WRAP )
			{
				col  = 0;
				row += 1;

				// erm...
				if ( row == editorState.nRows )
				{
					row -= 1;
				}
			}
			else
			{
				break;  // erm...
			}
		}

		setCursorPosition( row, col );


		//
		s += 1;
	}
}


// ____________________________________________________________________________________

void initEditor ( void )
{

	/* Hard code dimensions for now...
	   Dimensions based on GFXText.c
	*/
	editorState.nRows = 18;
	editorState.nCols = 40;
}


// ____________________________________________________________________________________

void drawRows ( void )
{
	uint  row;
	int   center;
	char* welcomeMsg;
	int   welcomeMsgLen;

	welcomeMsg    = "Kilo Editor";
	welcomeMsgLen = 11;  // excluding null terminal

	for ( row = 0; row < editorState.nRows; row += 1 )
	{
		setCursorPosition( row, 0 );

		eraseInLine( ERASELINE_ALL );  // erase entire line...

		printChar( '~' );

		// Print welcome message
		if ( row == editorState.nRows / 3 )
		{
			center = ( editorState.nCols / 2 ) - ( welcomeMsgLen / 2 );

			setCursorPosition( row, center );

			printString( welcomeMsg, NO_WRAP );
		}
	}
}

void refreshScreen ( void )
{
	// clearScreen();
	setCursorPosition( 0, 0 );

	drawRows();

	setCursorPosition( 0, 0 );
	drawCursor();
}


// ____________________________________________________________________________________

char readKey ( void )
{
	char c;

	if ( read( stdin, &c, 1 ) < 0 )
	{
		die( "read" );
	}

	printf( stdout, "(%d, %c)\n", c, c );

	return c;
}

void processKeyPress ( void )
{
	char c;

	c = readKey();

	switch ( c )
	{
		case CTRL( 'q' ):

			die( NULL );
	}
}









// ____________________________________________________________________________________

void setup ( void )
{
	initGFXText();

	//
	// enableRawMode();
	initEditor();

}

void cleanup ( void )
{
	exitGFXText();

	// disableRawMode();
}


void die ( char* msg )
{
	cleanup();

	if ( msg )
	{
		printf( stderr, "keditor: death by %s\n", msg );
	}

	exit();
}


// ____________________________________________________________________________________

int main ( void )
{
	setup();

	while ( 1 )
	{
		refreshScreen();

		processKeyPress();  // atm, blocks
	}

	cleanup();

	exit();
}
