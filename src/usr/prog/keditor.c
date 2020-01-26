/*
  Build your own text editor
  Jeremy Ruten
  https://viewsourcecode.org/snaptoken/kilo/index.html
*/

/* NOTE: QEMU window has to be in focus for
         navigation keys to work (UP/DOWN/HOME etc)
*/


/*

TODO:
	. 

*/


#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "termios.h"
#include "GFX.h"      // Render graphically
#include "GFXtext.h"  // Render graphically


// Default colors (standard 256-color VGA palette)
static uchar textColor          = 24;   // grey
static uchar textBgColor        = 15;   // white
static uchar cursorColor        = 112;  // brown
static uchar warningTextColor   = 15;   // white
static uchar warningTextBgColor = 12;   // red;

//
#define TABSIZE 3   // tab size in spaces


/* For now using fixed value. Ideally would use runtime value of
   'editorState.nScreenCols + 1'
*/
#define STATUSBUFSZ 81

/* Key macros from 'kbd.h'

   QEMU window has to be in focus to use the keys below.

   If the terminal/console/command-prompt that launched QEMU is
   instead in focus, you'll get a multibyte escape sequence that
   needs to be decoded. See explanation here:
    . https://viewsourcecode.org/snaptoken/kilo/03.rawInputAndOutput.html#arrow-keys

   I haven't bothered to do this decoding...
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
#define K_BACKSPACE  8
#define K_ESCAPE    27


//
#define CTRL( k ) ( ( k ) & 0x1F )  // https://en.wikipedia.org/wiki/ASCII#Control_characters


//
struct _textRow {

	int   len;    // does not include null-terminal
	char* chars;  // has space for null-terminal...

	/* What rendered might be different length than bytes on file.
	   For example tabs.
	*/
	int   len_render;    // does not include null-terminal
	char* chars_render;  // has space for null-terminal...
};

// typedef struct _textRow textRow;


//
struct _editorState {

	struct termios origConsoleAttr;

	// screen dimensions
	int nScreenRows;
	int nScreenCols;

	// edit cursor position
	int editCursorRow;
	int editCursorCol;

	// render cursor position
	int renderCursorRow;
	int renderCursorCol;

	//
	struct _textRow* textRows;  // better name
	int              nTextRows;
	int              textRowOffset;  // vertical scroll offset
	int              textColOffset;  // horizontal scroll offset

	//
	char* filename;
	char  message [ STATUSBUFSZ ];
	int   messageTime;
	int   messageIsWarning;  // will use fancy formatting to emphasize

	//
	int dirty;       // unsaved changes
	int promptQuit;  // prompt on quit with unsaved changes
};

static struct _editorState editorState;


//
void die ( char* );
void updateTextRowRender ( struct _textRow* );
void insertChar ( int );
void deleteChar ( void );
void appendTextRow ( char*, int );
void setMessage ( const char*, ... );
void refreshScreen ( void );


// ____________________________________________________________________________________

//
/*struct charBuffer {

	char* buf;
	int   len;
};*/

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

void eraseInLine ( int mode )
{
	int row;
	int col;
	int savedCol;

	// Invalid mode, ignore
	if ( mode > 2 )
	{
		return;
	}


	GFXText_getCursorPosition( &row, &col );

	savedCol = col;  // save


	if ( mode == ERASELINE_RIGHT )
	{
		while ( col < editorState.nScreenCols )
		{
			GFXText_setCursorPosition( row, col );
			GFXText_printChar( ' ' );

			col += 1;
		}
	}
	else if ( mode == ERASELINE_LEFT )
	{
		while ( col >= 0 )
		{
			GFXText_setCursorPosition( row, col );
			GFXText_printChar( ' ' );

			if ( col == 0 ) { break; }  // unsigned, avoid negatives...

			col -= 1;
		}
	}
	else if ( ERASELINE_ALL )
	{
		for ( col = 0; col < editorState.nScreenCols; col += 1 )
		{
			GFXText_setCursorPosition( row, col );
			GFXText_printChar( ' ' );
		}
	}


	// Restore
	GFXText_setCursorPosition( row, savedCol );
}

void temp_testEraseLine ( void )
{
	int col;
	int row;

	row = 0;

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
	{
		GFXText_setCursorPosition( row, col );
		GFXText_printChar( '$' );
	}

	GFXText_setCursorPosition( 0, 20 );

	eraseInLine( 0 );



	row = 1;

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
	{
		GFXText_setCursorPosition( row, col );
		GFXText_printChar( '&' );
	}

	GFXText_setCursorPosition( 1, 20 );

	eraseInLine( 1 );



	row = 2;

	for ( col = 0; col < editorState.nScreenCols; col += 1 )
	{
		GFXText_setCursorPosition( row, col );
		GFXText_printChar( '#' );
	}

	GFXText_setCursorPosition( 2, 20 );

	eraseInLine( 2 );
}


// ____________________________________________________________________________________

/* Here, since we're responsible for cursor movement...
*/
#define WRAP    0
#define NO_WRAP 1

void printString ( char* s, int wrap )
{
	int row;
	int col;


	GFXText_getCursorPosition( &row, &col );

	while ( *s )
	{
		//
		GFXText_printChar( *s );
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

		GFXText_setCursorPosition( row, col );


		//
		s += 1;
	}
}


// ____________________________________________________________________________________

















// ____________________________________________________________________________________

char* textRowsToString ( int* bufLen )
{
	struct _textRow* curTextRow;
	int              len;
	int              i;
	char*            buf;
	char*            p;

	// Determine total length
	len = 0;

	for ( i = 0; i < editorState.nTextRows; i += 1 )
	{
		curTextRow = editorState.textRows + i;

		len += curTextRow->len + 1;  // +1 for newline
	}

	*bufLen = len;


	// Create and fill buffer
	buf = ( char* ) malloc( len );

	p = buf;

	for ( i = 0; i < editorState.nTextRows; i += 1 )
	{
		curTextRow = editorState.textRows + i;

		memcpy( p, curTextRow->chars, curTextRow->len );

		p += curTextRow->len;

		*p = '\n';

		p += 1;
	}

	return buf;
}


void save ( void )
{
	int   len;
	char* buf;
	int   fd;
	int   nWritten;
	char* msg;

	if ( editorState.filename == NULL )
	{
		return;
	}
	
	buf = textRowsToString( &len );

	/* Instead of opening 'filename' with O_TRUNC (to zero), a
	   safer approach would be to first write to a temporary file.
	   This way, if the write fails, the original file still has
	   all its contents...
	*/
	fd = open( editorState.filename, O_RDWR | O_CREATE | O_TRUNC );

	if ( fd == - 1 )
	{
		msg = "Error: save failed (can't open)";
	}
	else
	{
		nWritten = write( fd, buf, len );

		if ( nWritten != len )
		{
			msg = "Error: save failed (can't write)";
		}
		else
		{
			msg = "File saved!";

			// Mark as up-to-date
			editorState.dirty = 0;
		}

		close( fd );
	}

	free( buf );

	setMessage( msg );
}


// ____________________________________________________________________________________

void freeTextRow ( struct _textRow* textRow )
{
	.
}

void deleteTextRow ( int idx )
{
	.
}

void appendStringToTextRow ( struct _textRow* textRow, char* s, int len )
{
	.
}


// ____________________________________________________________________________________

void deleteCharFromTextRow ( struct _textRow* textRow, int idx )
{
	/* Why is this check necessary?
	   What would trigger idx to be out of bounds?
	*/
	if ( ( idx < 0 ) || ( idx > textRow->len ) )
	{
		return;
	}

	// Move chars currently at idx+1..len to idx..len-1
	/* Ex:
	      start:
	         chars = [ a, b, c, 0 ], len = 3

	      delete:
	         1

	         chars = [ a, b, c, 0 ]
	                      ^  ^
	                      |  |--------- src: idx + 1 = 2
	                      |------------ dst: idx     = 1

	         n: len - idx = 2

	      end:
	         chars = [ a, c, 0, 0 ]
	*/
	memmove(

		textRow->chars + idx,
		textRow->chars + idx + 1,
		textRow->len - idx
	);

	// Update to reflect deletion
	textRow->len -= 1;


	// Update textRow's render fields...
	updateTextRowRender( textRow );
}

void deleteChar ( void )
{
	struct _textRow* curTextRow;

	// If cursor is at end of file, nothing to delete
	if ( editorState.editCursorRow < editorState.nTextRows )
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;
	}
	else
	{
		return;
	}


	// If there is a character to the left of the cursor, delete it
	if ( editorState.editCursorCol > 0 )
	{
		deleteCharFromTextRow( curTextRow, editorState.editCursorCol - 1 );

		// Update pos
		editorState.editCursorCol -= 1;
	}

	//
	else
	{
		.
	}


	// Mark dirty
	editorState.dirty += 1;
}


// ____________________________________________________________________________________

void insertCharIntoTextRow ( struct _textRow* textRow, int idx, int c )
{
	/* Why is this check necessary?
	   What would trigger idx to be out of bounds?
	*/
	if ( ( idx < 0 ) || ( idx > textRow->len ) )
	{
		idx = textRow->len;
	}

	// Grow buffer to hold new char
	textRow->chars = realloc( textRow->chars, textRow->len + 2 );  // +1 for c, +1 for null terminal

	// Move chars currently at idx..len+1, one char over to idx+1..len+2
	/* Ex:
	      start:
	         chars = [ a, b, c, 0 ], len = 3

	      realloc:
	         chars = [ a, b, c, 0, . ]

	      insert:
	         d, 1

	         chars = [ a, b, c, 0, . ]
	                      ^  ^
	                      |  |--------- dst: idx + 1 = 2
	                      |------------ src: idx     = 1

	         n: len + 1 - idx = 3

	      end:
	         chars = [ a, ., b, c, 0 ]
	*/
	memmove(

		textRow->chars + idx + 1,
		textRow->chars + idx,
		textRow->len + 1 - idx  // +1 for null terminal?
	);

	// Place c in array
	textRow->chars[ idx ] = c;

	// Update to reflect new space added for c
	textRow->len += 1;


	// Update textRow's render fields...
	updateTextRowRender( textRow );
}

void insertChar ( int c )
{
	struct _textRow* curTextRow;

	/* If cursor is at end of file, append a new text row before
	   we begin inserting a character
	*/
	if ( editorState.editCursorRow == editorState.nTextRows )
	{
		appendTextRow( "", 0 );
	}

	curTextRow = editorState.textRows + editorState.editCursorRow;

	insertCharIntoTextRow( curTextRow, editorState.editCursorCol, c );


	// Update pos
	editorState.editCursorCol += 1;

	// Mark dirty
	editorState.dirty += 1;
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

void appendTextRow ( char* line, int lineLen )
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

	// Mark dirty
	editorState.dirty += 1;
}


// ____________________________________________________________________________________

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


	// 'appendTextRow' sets dirty flag, clear it
	editorState.dirty = 0;


	//
	free( line );

	close( fd );
}


// ____________________________________________________________________________________

int convert_edit_to_renderCursorCol ( struct _textRow* textRow, int _editCursorCol )
{
	int _renderCursorCol;
	int  j;
	int nLeft;
	int nRight;

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

/* Snap cursor to end of line.
   I.e. if on row update, col position is now invalid (past end of line)
*/
void snapCursor ( void )
{
	struct _textRow* curTextRow;

	if ( editorState.editCursorRow < editorState.nTextRows )
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;
	}
	else
	{
		curTextRow = NULL;
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


void moveCursor ( uchar key )
{
	struct _textRow* curTextRow;

	if ( editorState.editCursorRow < editorState.nTextRows )
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;
	}
	else
	{
		curTextRow = NULL;
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


	// Snap cursor to end of line
	snapCursor();
}


void moveCursor2 ( uchar key )
{
	struct _textRow* curTextRow;
	int              offsetFromScreenTop;
	int              offsetFromScreenBtm;

	switch ( key )
	{
		/* TODO: simplify KEY_PAGEUP/DOWN logic
		*/
		case KEY_PAGEUP:

			// Get cursor's initial row offset
			offsetFromScreenTop = editorState.editCursorRow - editorState.textRowOffset;
			offsetFromScreenBtm = editorState.nScreenRows - offsetFromScreenTop - 1;


			// Move cursor to top of screen
			editorState.editCursorRow -= offsetFromScreenTop;

			/* If we have gone past or reached the top of the file,
			   there is nothing to page-up to... Exit early.
			*/
			if ( editorState.editCursorRow <= 0 )
			{
				//
				editorState.editCursorRow = 0;

				// Update cursor's column offset
				snapCursor();

				break;
			}


			// Scroll visible text up a page...
			editorState.editCursorRow -= editorState.nScreenRows;

			if ( editorState.editCursorRow < 0 )
			{
				editorState.editCursorRow = 0;
			}

			scroll();


			// Restore cursor's initial row offset
			editorState.editCursorRow += offsetFromScreenTop;

			if ( editorState.editCursorRow > editorState.nTextRows )
			{
				editorState.editCursorRow = editorState.nTextRows;
			}

			// Update cursor's column offset
			snapCursor();

			break;

		case KEY_PAGEDOWN:

			// Get cursor's initial row offset
			offsetFromScreenTop = editorState.editCursorRow - editorState.textRowOffset;
			offsetFromScreenBtm = editorState.nScreenRows - offsetFromScreenTop - 1;


			// Move cursor to bottom of screen
			editorState.editCursorRow += offsetFromScreenBtm;

			/* If we have gone past or reached the last line of the file,
			   there is nothing to page-down to... Exit early.
			*/
			if ( editorState.editCursorRow >= editorState.nTextRows )
			{
				//
				editorState.editCursorRow = editorState.nTextRows;

				// Update cursor's column offset
				snapCursor();

				break;
			}


			// Scroll visible text down a page...
			editorState.editCursorRow += editorState.nScreenRows;

			if ( editorState.editCursorRow > editorState.nTextRows )
			{
				editorState.editCursorRow = editorState.nTextRows;
			}

			scroll();


			// Restore cursor's initial row offset
			editorState.editCursorRow -= offsetFromScreenBtm;

			if ( editorState.editCursorRow < 0 )
			{
				editorState.editCursorRow = 0;
			}

			// Update cursor's column offset
			snapCursor();

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

		default:

			break;
	}
}


// ____________________________________________________________________________________

char readKey ( void )
{
	char c;

	if ( read( stdin, &c, 1 ) < 0 )  // read blocks
	{
		die( "read" );
	}

	// printf( stdout, "(%d, %c)\n", c, c );

	return c;
}

void processKeyPress ( void )
{
	uchar c;

	c = ( uchar ) readKey();

	switch ( c )
	{
		case CTRL( 'q' ):

			if ( editorState.dirty && editorState.promptQuit )
			{
				setMessage( "Unsaved changes. <Ctrl-q> again to quit" );
				editorState.messageIsWarning = 1;

				//
				editorState.promptQuit = 0;

				// Force screen refresh so message appears
				refreshScreen();

				//
				return;
			}
			else
			{
				die( NULL );
				break;
			}

		case CTRL( 's' ):

			save();
			break;

		case CTRL( 'g' ):  // ctrl-h clashing with backspace

			// TODO
			break;


		case CTRL( 'f' ):

			// TODO
			break;


		case KEY_LEFT:
		case KEY_RIGHT:
		case KEY_UP:
		case KEY_DOWN:

			moveCursor( c );
			break;


		case KEY_PAGEUP:
		case KEY_PAGEDOWN:
		case KEY_HOME:
		case KEY_END:

			moveCursor2( c );
			break;


		case '\n':

			// TODO
			break;


		case KEY_DELETE:
		case K_BACKSPACE:

			if ( c == KEY_DELETE )
			{
				moveCursor( KEY_RIGHT );
			}

			deleteChar();

			break;


		case K_ESCAPE:

			break;


		default:

			insertChar( c );
			break;
	}


	//
	editorState.promptQuit = 1;
}


// ____________________________________________________________________________________

void drawRows ( void )
{
	int              screenRow;
	int              fileRow;
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
			GFXText_setCursorPosition( screenRow, 0 );

			GFXText_printChar( '~' );

			// Print welcome message
			if ( screenRow == editorState.nScreenRows / 3 )
			{
				centerCol = ( editorState.nScreenCols / 2 ) - ( welcomeMsgLen / 2 );

				GFXText_setCursorPosition( screenRow, centerCol );

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
			GFXText_setCursorPosition( screenRow, 0 );

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
				GFXText_printChar( '~' );
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
	char statusMsg [ STATUSBUFSZ ];

	GFXText_invertTextColors();


	// Draw dirty marker
	if ( editorState.dirty )
	{
		GFXText_setCursorPosition( editorState.nScreenRows, 0 );

		GFXText_printChar( '*' );

		slen = 1;
	}
	else
	{
		slen = 0;
	}


	// Draw file info
	/* TODO: either show just filename (exclude path),
	   or scroll animation to show full path...
	*/
	slen2 = snprintf(

		statusMsg,
		editorState.nScreenCols + 1,  // +1 for null terminal

		"\"%s\" %dL",
		editorState.filename ? editorState.filename : "Untitled",
		editorState.nTextRows
	);

	GFXText_setCursorPosition( editorState.nScreenRows, slen );

	printString( statusMsg, NO_WRAP );

	slen += slen2;


	// Get cursor position
	slen2 = snprintf(

		statusMsg,
		editorState.nScreenCols + 1,  // +1 for null terminal

		"%dL,%dC",
		editorState.renderCursorRow + 1,  // +1 for one-index display
		editorState.renderCursorCol + 1   // +1 for one-index display
	);

	posTextCol = editorState.nScreenCols - slen2;  // right align


	// Draw the rest...
	for ( col = slen; col < editorState.nScreenCols; col += 1 )
	{
		GFXText_setCursorPosition( editorState.nScreenRows, col );

		// Draw spaces
		if ( col < posTextCol )
		{
			GFXText_printChar( ' ' );
		}

		// Draw cursor position
		else
		{
			printString( statusMsg, NO_WRAP );

			break;
		}
	}


	// Restore
	GFXText_invertTextColors();
}

void setMessage ( const char* fmt, ... )
{
	va_list argp;

	va_start( argp, fmt );

	vsnprintf( editorState.message, STATUSBUFSZ, fmt, argp );

	va_end( argp );


	editorState.messageTime = uptime();  // "ticks" since kernel started...
	// editor.messageTime = time();      // seconds since epoch


	editorState.messageIsWarning = 0;  // Hmmm... for now set default here
}

void drawMessageBar ( void )
{
	int   curTime;
	uchar curTextColor;
	uchar curTextBgColor;

	curTime = uptime();

	// Display message if less than x ticks old
	if ( ( curTime - editorState.messageTime ) < 300 )
	{
		// Change colors
		if ( editorState.messageIsWarning )
		{
			curTextColor   = GFXText_getTextColor(); 
			curTextBgColor = GFXText_getTextBgColor();

			GFXText_setTextColor( warningTextColor );
			GFXText_setTextBgColor( warningTextBgColor );
		}

		GFXText_setCursorPosition( editorState.nScreenRows + 1, 0 );

		printString( editorState.message, NO_WRAP );

		// Restore colors
		if ( editorState.messageIsWarning )
		{
			GFXText_setTextColor( curTextColor );
			GFXText_setTextBgColor( curTextBgColor );
		}
	}
}


// ____________________________________________________________________________________

void refreshScreen ( void )
{
	//
	GFXText_clearScreen();

	//
	scroll();

	//
	drawRows();
	drawStatusBar();
	drawMessageBar();

	//
	GFXText_setCursorPosition(

		editorState.renderCursorRow - editorState.textRowOffset,
		editorState.renderCursorCol - editorState.textColOffset
	);
	GFXText_drawCursor();
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
	GFXText_getDimensions( &editorState.nScreenRows, &editorState.nScreenCols );

	editorState.nScreenRows -= 1;  // reserve space for status bar
	editorState.nScreenRows -= 1;  // reserve space for message bar


	//
	editorState.nTextRows     = 0;
	editorState.textRows      = NULL;
	editorState.textRowOffset = 0;
	editorState.textColOffset = 0;


	//
	editorState.filename     = NULL;
	editorState.message[ 0 ] = 0;
	editorState.messageTime  = 0;


	//
	editorState.dirty      = 0;
	editorState.promptQuit = 1;
}


// ____________________________________________________________________________________

void setup ( void )
{
	GFX_init();

	GFXText_setTextColor( textColor );
	GFXText_setTextBgColor( textBgColor );
	GFXText_setCursorColor( cursorColor );
	GFXText_clearScreen();

	enableRawMode();

	initEditor();
}

void cleanup ( void )
{
	GFX_exit();

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

	setMessage( "<Ctrl-q> quit, <Ctrl-g> help" );

	while ( 1 )
	{
		refreshScreen();

		processKeyPress();  // atm, blocks
	}

	cleanup();

	exit();
}
