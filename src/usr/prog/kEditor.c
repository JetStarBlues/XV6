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
#include "fcntl.h"
#include "GFXtext.h"  // Render graphically
#include "termios.h"


//
#define CTRL( k ) ( ( k ) & 0x1F )  // https://en.wikipedia.org/wiki/ASCII#Control_characters


//
struct _textRow {

	int   len;    // not null-terminated
	char* chars;  // better name
};

// typedef struct _textRow textRow;


//
struct _editorState {

	struct termios origConsoleAttr;

	// screen dimensions
	uint nScreenRows;
	uint nScreenCols;

	// cursor position
	uint cursorRow;
	uint cursorCol;

	//
	struct _textRow* textRows;  // better name
	int              nTextRows;
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

void enableRawMode ( void )
{
	struct termios newConsoleAttr;

	if ( getConsoleAttr( stdin, &( editorState.origConsoleAttr ) ) < 0 )
	{
		die( "getConsoleAttr" );
	}

	/* Copy editorState.origConsoleAttr to newConsoleAttr */
	// newConsoleAttr = editorState.origConsoleAttr;
	memcpy( &newConsoleAttr, &( editorState.origConsoleAttr ), sizeof( struct termios ) );

	newConsoleAttr.echo   = 0;  // disable echoing
	newConsoleAttr.icanon = 0;  // disiable canonical mode input

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
		while ( col < editorState.nScreenCols )
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
		for ( col = 0; col < editorState.nScreenCols; col += 1 )
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

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
	{
		setCursorPosition( row, col );
		printChar( '$' );
	}

	setCursorPosition( 0, 20 );

	eraseInLine( 0 );



	row = 1;

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
	{
		setCursorPosition( row, col );
		printChar( '&' );
	}

	setCursorPosition( 1, 20 );

	eraseInLine( 1 );



	row = 2;

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
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

		// Handle horizontal overflow... erm...
		if ( col == editorState.nScreenCols )
		{
			if ( wrap == WRAP )
			{
				col  = 0;
				row += 1;

				// Handle vertical overflow... erm...
				if ( row == editorState.nScreenRows )
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

void appendTextRow ( char*line, int lineLen )
{
	struct _textRow* p;
	struct _textRow* curTextRow;

	// Allocate space for a new textRow
	p = realloc(

		editorState.textRows,
		sizeof( struct _textRow ) * ( editorState.nTextRows + 1 )
	);

	if ( p == NULL )
	{
		die( "realloc" );
	}

	editorState.textRows = p;


	// Copy the line to the textRow
	curTextRow = editorState.textRows + editorState.nTextRows;

	curTextRow->chars = malloc( lineLen + 1 );  // +1 for null temrinal

	memcpy( curTextRow->chars, line, lineLen );

	curTextRow->chars[ lineLen ] = 0;  // null terminate

	//
	curTextRow->len = lineLen;

	// Update count
	editorState.nTextRows += 1;
}


void openFile ( char* filename )  // better name
{
	int   fd;
	char* line;
	int   lineLen;  // excluding null terminal
	uint  lineBufSize;

	//
	fd = open( filename, O_RDONLY );

	if ( fd < 0 )
	{
		die( "open" );
	}

	//
	line        = NULL;
	lineLen     = 0;
	lineBufSize = 0;

	while ( 1 )
	{
		// Use 'getline' to read a line from the file
		lineLen = getline( &line, &lineBufSize, fd );
		// printf( 1, "getline returned: (%d) %s\n", lineLen, line );

		//
		if ( lineLen < 0 )
		{
			break;
		}

		// Strip newline characters returned by 'getline'
		while ( ( lineLen > 0 ) &&
		        ( ( line[ lineLen - 1 ] == '\n' ) || ( line[ lineLen - 1 ] == '\r' ) ) )
		{
			lineLen -= 1;
		}

		//
		appendTextRow( line, lineLen );
	}

	//
	free( line );

	close( fd );
}







// ____________________________________________________________________________________

/* Key macros from 'kbd.h'

   QEMU window has to be in focus to use the associated keys.

   If the terminal/console/command-prompt that launched QEMU is
   instead in focus, you'll get a multibyte escape sequence that
   needs to be decoded. See explanation here:
    . https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#arrow-keys 

   I think the code in 'kbd.c' does this decoding for you...
   TODO: confirm 
*/
#define KEY_HOME     0xE0  // Doesn't seem to work =(
#define KEY_END      0xE1
#define KEY_UP       0xE2
#define KEY_DOWN     0xE3
#define KEY_LEFT     0xE4
#define KEY_RIGHT    0xE5
#define KEY_PAGEUP   0xE6
#define KEY_PAGEDOWN 0xE7
#define KEY_DELETE   0xE9

//
void moveCursor ( uchar key )
{
	switch ( key )
	{
		case KEY_LEFT:

			if ( editorState.cursorCol != 0 )
			{
				editorState.cursorCol -= 1;
			}
			break;

		case KEY_RIGHT:

			if ( editorState.cursorCol < editorState.nScreenCols - 1 )
			{
				editorState.cursorCol += 1;
			}
			break;

		case KEY_UP:

			if ( editorState.cursorRow != 0 )
			{
				editorState.cursorRow -= 1;
			}
			break;

		case KEY_DOWN:

			if ( editorState.cursorRow < editorState.nScreenRows - 1 )
			{
				editorState.cursorRow += 1;
			}
			break;
	}
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
	uchar c;

	c = ( uchar ) readKey();

	switch ( c )
	{
		case CTRL( 'q' ):

			die( NULL );
			break;

		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_UP:
		case KEY_DOWN:

			moveCursor( c );
			break;

		case KEY_PAGEUP:
		case KEY_PAGEDOWN:
		{
			// For now, just move to vertical extremes
			int n = editorState.nScreenRows;
			while ( n )
			{
				moveCursor( c == KEY_PAGEUP ? KEY_UP : KEY_DOWN );
				n -= 1;
			}

			break;
		}

		case KEY_HOME:
		case KEY_END:
		{
			// For now, just move to horizontal extremes
			int n = editorState.nScreenCols;
			while ( n )
			{
				moveCursor( c == KEY_HOME ? KEY_LEFT : KEY_RIGHT );
				n -= 1;
			}

			break;
		}

		// case KEY_DELETE:
			// break;
	}
}


// ____________________________________________________________________________________

void drawRows ( void )
{
	uint             row;
	int              center;
	char*            welcomeMsg;
	int              welcomeMsgLen;
	struct _textRow* curTextRow;

	welcomeMsg    = "Kilo Editor";
	welcomeMsgLen = 11;  // excluding null terminal

	setCursorPosition( 0, 0 );  // makes more sense here...

	for ( row = 0; row < editorState.nScreenRows; row += 1 )
	{
		setCursorPosition( row, 0 );

		eraseInLine( ERASELINE_ALL );  // erase entire line...

		// Draw file contents...
		if ( row < editorState.nTextRows )
		{
			curTextRow = editorState.textRows + row;

			printString( curTextRow->chars, NO_WRAP );
		}
		else
		{
			printChar( '~' );
		}

		// Print welcome message
		if ( ( editorState.nTextRows == 0 ) && ( row == editorState.nScreenRows / 3 ) )
		{
			center = ( editorState.nScreenCols / 2 ) - ( welcomeMsgLen / 2 );

			setCursorPosition( row, center );

			printString( welcomeMsg, NO_WRAP );
		}
	}
}

void refreshScreen ( void )
{
	// clearScreen();

	// setCursorPosition( 0, 0 );
	// Does editor cursor mirror terminal's ???

	drawRows();

	// setCursorPosition( 0, 0 );
	setCursorPosition( editorState.cursorRow, editorState.cursorCol );
	drawCursor();
}


// ____________________________________________________________________________________

void initEditor ( void )
{
	//
	editorState.cursorCol = 0;
	editorState.cursorRow = 0;

	//
	getDimensions( &editorState.nScreenRows, &editorState.nScreenCols );

	//
	editorState.nTextRows = 0;
	editorState.textRows  = NULL;
}


// ____________________________________________________________________________________

void setup ( void )
{
	initGFXText();

	enableRawMode();

	initEditor();
}

void cleanup ( void )
{
	exitGFXText();

	disableRawMode();
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

int main ( int argc, char* argv [] )
{
	setup();

	if ( argc == 2 )
	{
		openFile( argv[ 1 ] );
	}

	while ( 1 )
	{
		refreshScreen();

		processKeyPress();  // atm, blocks
	}

	cleanup();

	exit();
}
