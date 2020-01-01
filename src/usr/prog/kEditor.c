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
#define TABSIZE 3  // tab size in spaces


//
#define CTRL( k ) ( ( k ) & 0x1F )  // https://en.wikipedia.org/wiki/ASCII#Control_characters


//
struct _textRow {

	int   len;    // not null-terminated
	char* chars;  // better name

	/* What rendered might be different length than bytes on file.
	   For example tabs.
	*/
	int   len_render;    // not null-terminated
	char* chars_render;
};

// typedef struct _textRow textRow;


//
struct _editorState {

	struct termios origConsoleAttr;

	// screen dimensions
	uint nScreenRows;
	uint nScreenCols;

	// edit cursor position
	uint editCursorRow;
	uint editCursorCol;

	// render cursor position
	uint renderCursorRow;
	uint renderCursorCol;

	//
	struct _textRow* textRows;  // better name
	int              nTextRows;
	int              textRowOffset;  // vertical scroll offset
	int              textColOffset;  // horizontal scroll offset

	//
	char* filename;
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
		// printf( 1, "%c", *s );


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

void updateTextRowRender ( struct _textRow* textRow )  // better name
{
	int j;
	int idx;
	int nTabs;

	// Count number of tabs in line
	for ( j = 0; j < textRow->len; j += 1 )
	{
		if ( textRow->chars[ j ] == '\t' )
		{
			nTabs += 1;
		}
	}


	//
	free( textRow->chars_render );  // hmm...

	textRow->chars_render = malloc(

		textRow->len            +
		nTabs * ( TABSIZE - 1 ) +  // space taken up by tabs
		1                          // null temrinal
	);


	//
	idx = 0;

	for ( j = 0; j < textRow->len; j += 1 )
	{
		if ( textRow->chars[ j ] == '\t' )
		{
			// Each tab must advane the cursor forward at least one column
			textRow->chars_render[ idx ] = ' ';

			idx += 1;


			// Append spaces until we get to a tab stop (column divisible by TABSIZE)
			while ( ( idx % TABSIZE ) != 0 )
			{
				textRow->chars_render[ idx ] = ' ';

				idx += 1;
			}
		}
		else
		{
			textRow->chars_render[ idx ] = textRow->chars[ j ];

			idx += 1;
		}
	}

	textRow->chars_render[ idx ] = 0;  // null terminate

	textRow->len_render = idx;
}


// ____________________________________________________________________________________

void appendTextRow ( char*line, int lineLen )
{
	struct _textRow* ptr;
	struct _textRow* curTextRow;

	// Allocate space for a new textRow
	ptr = realloc(

		editorState.textRows,
		sizeof( struct _textRow ) * ( editorState.nTextRows + 1 )
	);

	if ( ptr == NULL )
	{
		die( "realloc" );
	}

	editorState.textRows = ptr;


	// Copy the line to the textRow
	curTextRow = editorState.textRows + editorState.nTextRows;

	curTextRow->chars = malloc( lineLen + 1 );  // +1 for null temrinal

	memcpy( curTextRow->chars, line, lineLen );

	curTextRow->chars[ lineLen ] = 0;  // null terminate


	//
	curTextRow->len = lineLen;


	//
	curTextRow->chars_render = NULL;
	curTextRow->len_render   = 0;
	updateTextRowRender( curTextRow );


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
	free( editorState.filename );
	editorState.filename = strdup( filename );

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

uint convert_edit_to_renderCursorCol ( struct _textRow* textRow, uint _editCursorCol )
{
	uint _renderCursorCol;
	int  j;
	uint nLeft;
	uint nRight;

	_renderCursorCol = 0;

	for ( j = 0; j < _editCursorCol; j += 1 )
	{
		if ( textRow->chars[ j ] == '\t' )
		{
			// Number of columns we are to the right of the previous tab stop...
			nRight = _renderCursorCol % TABSIZE;

			// Number of columns we are to the left of the next tab stop...
			nLeft = ( TABSIZE - 1 ) - nRight;

			//
			_renderCursorCol += nLeft;
		}

		_renderCursorCol += 1;
	}

	return _renderCursorCol;
}

void scroll ( void )
{
	struct _textRow* curTextRow;

	/* Calculate location of render cursor...
	*/
	editorState.renderCursorRow = editorState.editCursorRow;

	if ( editorState.editCursorRow < editorState.nTextRows )
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;

		editorState.renderCursorCol = convert_edit_to_renderCursorCol(

			curTextRow,
			editorState.editCursorCol
		);
	}
	else
	{
		editorState.renderCursorCol = 0;
	}


	/* Vertical scroll */

	// Cursor above visible window, scroll up to cursor
	if ( editorState.renderCursorRow < editorState.textRowOffset )
	{
		editorState.textRowOffset = editorState.renderCursorRow;
	}

	// Cursor below visible window, scroll down to cursor
	if ( editorState.renderCursorRow >= ( editorState.nScreenRows + editorState.textRowOffset ) )
	{
		editorState.textRowOffset = editorState.renderCursorRow - editorState.nScreenRows + 1;  // why +1 ??
	}


	/* Horizontal scroll */

	// Cursor left of visible window, scroll right to cursor
	if ( editorState.renderCursorCol < editorState.textColOffset )
	{
		editorState.textColOffset = editorState.renderCursorCol;
	}

	// Cursor right of visible window, scroll left to cursor
	if ( editorState.renderCursorCol >= ( editorState.nScreenCols + editorState.textColOffset ) )
	{
		editorState.textColOffset = editorState.renderCursorCol - editorState.nScreenCols + 1;  // why +1 ??
	}
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
	struct _textRow* curTextRow;

	if ( editorState.editCursorRow >= editorState.nTextRows )
	{
		curTextRow = NULL;
	}
	else
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;
	}


	switch ( key )
	{
		case KEY_LEFT:

			if ( editorState.editCursorCol != 0 )
			{
				editorState.editCursorCol -= 1;
			}

			/* Allow ability to move to end of previous line
			*/
			else if ( editorState.editCursorRow > 0 )
			{
				editorState.editCursorRow -= 1;

				curTextRow = editorState.textRows + editorState.editCursorRow;

				editorState.editCursorCol = curTextRow->len;
			}

			break;

		case KEY_RIGHT:

			/* Prevent cursor from moving more than one char past end of line...

			   One char past allowed so that user can insert text at end of line...
			*/
			if ( curTextRow && ( editorState.editCursorCol < curTextRow->len ) )
			{
				editorState.editCursorCol += 1;
			}

			/* Allow ability to move to start of next line
			*/
			else if ( curTextRow && ( editorState.editCursorCol == curTextRow->len ) )
			{
				editorState.editCursorRow += 1;

				editorState.editCursorCol = 0;
			}

			break;

		case KEY_UP:

			if ( editorState.editCursorRow != 0 )
			{
				editorState.editCursorRow -= 1;
			}

			break;

		case KEY_DOWN:

			/* Prevent cursor from moving more than one line past end of file...

			   One line past allowed so that user can insert new lines at end of file...
			*/
			if ( editorState.editCursorRow < editorState.nTextRows )
			{
				editorState.editCursorRow += 1;
			}

			break;

		default:

			break;
	}


	/* Snap cursor to end of line.
	   I.e. if on row update, col position is now invalid (past end of line)
	*/

	if ( editorState.editCursorRow >= editorState.nTextRows )
	{
		curTextRow = NULL;
	}
	else
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;
	}

	if ( curTextRow )
	{
		if ( editorState.editCursorCol > curTextRow->len )  // past end of line
		{
			editorState.editCursorCol = curTextRow->len;
		}
	}
	else  // empty line
	{
		editorState.editCursorCol = 0;
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

	// printf( stdout, "(%d, %c)\n", c, c );

	return c;
}

void processKeyPress ( void )
{
	uchar            c;
	int              n;
	struct _textRow* curTextRow;
	uint             savedEditCursorRow;

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

			/* To scroll up or down a page, we position the cursor
			   at either the top or bottom of the screen, then simulate
			   nScreenRows key presses
			*/
			//
			savedEditCursorRow = editorState.editCursorRow;

			//
			if ( c == KEY_PAGEUP )
			{
				editorState.editCursorRow = editorState.textRowOffset;
			}
			else
			{
				editorState.editCursorRow = editorState.textRowOffset + editorState.nScreenRows - 1;

				if ( editorState.editCursorRow > editorState.nTextRows )
				{
					editorState.editCursorRow = editorState.nTextRows;
				}
			}

			//
			n = editorState.nScreenRows;

			while ( n )
			{
				moveCursor( c == KEY_PAGEUP ? KEY_UP : KEY_DOWN );
				n -= 1;
			}

			// JK, calling moveCursor seems pointless...

			// JK FIX ME
			editorState.editCursorRow = savedEditCursorRow + editorState.nScreenRows;

			break;

		case KEY_HOME:

			editorState.editCursorCol = 0;

			break;

		case KEY_END:

			if ( editorState.editCursorRow < editorState.nTextRows )
			{
				curTextRow = editorState.textRows + editorState.editCursorRow;

				editorState.editCursorCol = curTextRow->len;
			}

			break;

		// case KEY_DELETE:
			// break;

		default:

			break;
	}
}


// ____________________________________________________________________________________

void drawRows ( void )
{
	uint             screenRow;
	uint             fileRow;
	struct _textRow* curTextRow;
	char*            text;
	int              centerCol;
	char*            welcomeMsg;
	int              welcomeMsgLen;

	welcomeMsg    = "Kilo Editor";
	welcomeMsgLen = 11;  // excluding null terminal


	// Empty file
	if ( editorState.nTextRows == 0 )
	{
		for ( screenRow = 0; screenRow < editorState.nScreenRows; screenRow += 1 )
		{
			setCursorPosition( screenRow, 0 );

			printChar( '~' );

			// Print welcome message
			if ( screenRow == editorState.nScreenRows / 3 )
			{
				centerCol = ( editorState.nScreenCols / 2 ) - ( welcomeMsgLen / 2 );

				setCursorPosition( screenRow, centerCol );

				printString( welcomeMsg, NO_WRAP );
			}
		}
	}


	// Non-empty file
	else
	{
		for ( screenRow = 0; screenRow < editorState.nScreenRows; screenRow += 1 )
		{
			//
			setCursorPosition( screenRow, 0 );

			//
			fileRow = screenRow + editorState.textRowOffset;

			// Draw file contents...
			if ( fileRow < editorState.nTextRows )
			{
				//
				curTextRow = editorState.textRows + fileRow;

				/* If haven't scrolled horizontally past end of line,
				   draw the visible part...
				*/
				if ( editorState.textColOffset < curTextRow->len_render )
				{
					text = curTextRow->chars_render + editorState.textColOffset;

					printString( text, NO_WRAP );
				}
			}
			else
			{
				printChar( '~' );
			}
		}
	}
}

void drawStatusBar ( void )
{
	int  col;
	int  posTextCol;
	int  slen;
	int  slen2;
	char status [ 81 ];  /* if nScreenCols was compile time constant, would use it
	                        instead to allocate space */

	invertTextColors();


	// Draw file info
	slen = snprintf(

		status,
		editorState.nScreenCols + 1,  // +1 for null terminal

		"\"%s\" %dL",
		editorState.filename ? editorState.filename : "Untitled",
		editorState.nTextRows
	);

	setCursorPosition( editorState.nScreenRows, 0 );

	printString( status, NO_WRAP );


	// Get cursor position
	slen2 = snprintf(

		status,
		editorState.nScreenCols + 1,  // +1 for null terminal

		"%dL,%dC",
		editorState.renderCursorRow + 1,  // +1 for one-index display
		editorState.renderCursorCol + 1   // +1 for one-index display
	);

	posTextCol = editorState.nScreenCols - slen2;  // right align


	// Draw the rest...
	if ( ( slen + slen2 ) < editorState.nScreenCols )
	{
		for ( col = slen; col < editorState.nScreenCols; col += 1 )
		{
			setCursorPosition( editorState.nScreenRows, col );

			// Draw spaces
			if ( col < posTextCol )
			{
				printChar( ' ' );
			}

			// Draw cursor position
			else
			{
				printString( status, NO_WRAP );

				break;
			}
		}
	}


	// Restore
	invertTextColors();
}


// ____________________________________________________________________________________

void refreshScreen ( void )
{
	//
	clearScreen();

	//
	scroll();

	//
	drawRows();
	drawStatusBar();

	//
	setCursorPosition(

		editorState.renderCursorRow - editorState.textRowOffset,
		editorState.renderCursorCol - editorState.textColOffset
	);
	drawCursor();
}


// ____________________________________________________________________________________

void initEditor ( void )
{
	//
	editorState.editCursorCol   = 0;
	editorState.editCursorRow   = 0;
	editorState.renderCursorCol = 0;
	editorState.renderCursorRow = 0;


	//
	getDimensions( &editorState.nScreenRows, &editorState.nScreenCols );

	editorState.nScreenRows -= 1;  // reserve space for status bar


	//
	editorState.nTextRows     = 0;
	editorState.textRows      = NULL;
	editorState.textRowOffset = 0;
	editorState.textColOffset = 0;


	//
	editorState.filename = NULL;
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
