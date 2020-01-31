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
    . Highlight search match

    . Text wrapping

    . Mouse select
       . Plus thing where auto scrolls

    . Copy and paste

    . Rearrange code
*/


#include "types.h"
#include "user.h"
#include "fcntl.h"
#include "termios.h"
#include "GFX.h"      // Render graphically
#include "GFXtext.h"  // Render graphically


// Default colors (standard 256-color VGA palette)
static uchar textColor          = 0x18;  // grey
static uchar textBgColor        = 0x0F;  // white
static uchar cursorColor        = 0x70;  // brown
static uchar warningTextColor   = 0x0F;  // white
static uchar warningTextBgColor = 0x0C;  // red
static uchar promptTextColor    = 0x18;  // grey
static uchar promptTextBgColor  = 0x5C;  // light yellow

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


// Help message
static char* helpMsg [] = {

	"-- Shortcuts --",
	"",
	". [ctrl-g] to view help (this page)",
	"    . [esc] to cancel",
	// "",
	". [ctrl-s] to save",
	"    . [esc] to cancel",
	// "",
	". [ctrl-t] to save as",
	"    . [esc]   to cancel",
	"    . [enter] to accept",
	// "",
	". [ctrl-f] to find",
	"    . [esc]   to cancel",
	"    . [arrow] to jump to next/prev match",
	"    . [enter] to accept",
	// "",
	". [ctrl-q] to quit",
};

static int helpMsgLen = sizeof( helpMsg ) / sizeof( char* );


//
struct _textRow {

	int   len;    // excluding null-terminal
	char* chars;  // null terminated

	/* What rendered might be different length than bytes on file.
	   For example tabs.
	*/
	int   len_render;    // excluding null-terminal
	char* chars_render;  // null terminated
};

// typedef struct _textRow textRow;


//
#define SEARCHDIR_FORWARD      1    // increment
#define SEARCHDIR_BACKWARD ( - 1 )  // decrement

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
	int   messageIsPrompt;   // will use fancy formatting to emphasize

	//
	int dirty;       // unsaved changes
	int promptQuit;  // prompt on quit with unsaved changes

	//
	int searchDirection;
	int lastMatchRowIdx;  // -1 if no last match
	int lastMatchColIdx;
	int lastMatchLen;
};

static struct _editorState editorState;


//
void die ( char* );
void updateTextRowRender ( struct _textRow* );
void insertChar ( uchar );
void deleteChar ( void );
void insertTextRow ( int, char*, int );
void setMessage ( const char*, ... );
char* promptUser (
	char*,
	void ( * ) ( char*, int, uchar )
);
void refreshScreen ( void );
int convert_render_to_editCursorCol ( struct _textRow*, int );
void scroll ( void );
void drawHelpScreen ( void );


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

/* Returns a pointer to the beginning of the last occurrence
   of substring 'sub' in string 's'.
   If not found, returns NULl.

   'slen' as parameter because...
*/ 
char* reverseStrstr ( char* sEnd, int searchLen, char* sub )
{
	int subLen;
	int i;
	int j;
	int jLast;
	int n;

	// Early exit if substring is empty
	if ( *sub == 0 )
	{
		return NULL;
	}

	//
	subLen = strlen( sub );
	jLast  = subLen - 1;

	//
	i = 1;

	while ( i <= searchLen )
	{
		// If matches last char of substring, check for full match
		if ( *( sEnd - i ) == *( sub + jLast ) )
		{
			i += 1;
			j = jLast - 1;

			// Check if matches other chars
			n = 1;

			while ( j >= 0 )
			{
				// Reached end of search without finding a match
				if ( i > searchLen )
				{
					return NULL;
				}

				// Mismatch before end of substring
				if ( *( sEnd - i ) != *( sub + j ) )
				{
					break;
				}

				// Char matched, evaluate next
				n += 1;
				i += 1;
				j -= 1;
			}

			// If all chars matched, return pointer to beginning of match in string
			if ( n == subLen )
			{
				return sEnd - i + 1;
			}
		}

		// Else, evaluate next char
		else
		{
			i += 1;
		}
	}

	// Reached end of search without finding a match
	return NULL;
}


// Incremental search achieved by using callback
void findCallback ( char* query, int queryLen, uchar key )
{
	struct _textRow* textRow;
	int              rowIdx;
	int              colIdx;
	char*            match;
	int              i;
	char*            startPos;
	int              searchLen;

	// Configure search direction
	if ( ( key == KEY_RIGHT ) || ( key == KEY_DOWN ) )
	{
		editorState.searchDirection = SEARCHDIR_FORWARD;
	}
	else if ( ( key == KEY_LEFT ) || ( key == KEY_UP ) )
	{
		editorState.searchDirection = SEARCHDIR_BACKWARD;
	}
	else
	{
		editorState.lastMatchRowIdx = - 1;                // clear
		editorState.lastMatchColIdx = - 1;                // clear
		editorState.searchDirection = SEARCHDIR_FORWARD;  // restore default
	}


	// If user has left search mode, return early
	if ( ( key == '\n' ) || ( key == K_ESCAPE ) )
	{
		return;
	}


	// Perform search
	match  = NULL;
	rowIdx = editorState.lastMatchRowIdx;
	colIdx = editorState.lastMatchColIdx;


	// Search remainder of previous row for another match
	if ( rowIdx != - 1 )
	{
		textRow = editorState.textRows + rowIdx;

		// Find next occurrence in row
		if ( editorState.searchDirection == SEARCHDIR_FORWARD )
		{
			startPos = textRow->chars_render + colIdx + editorState.lastMatchLen;

			match = strstr( startPos, query	);
		}
		// Find previous occurrence in row
		else
		{
			startPos = textRow->chars_render + colIdx;

			searchLen = textRow->len_render - strlen( startPos );

			match = reverseStrstr( startPos, searchLen, query );
		}
	}


	// Search next rows for match
	if ( match == NULL )
	{
		// Start search at cursor location
		if ( rowIdx == - 1 )
		{
			if ( editorState.searchDirection == SEARCHDIR_FORWARD )
			{
				rowIdx = editorState.editCursorRow - 1;  // will be +1'd below
			}
			else
			{
				rowIdx = editorState.editCursorRow + 1;  // will be -1'd below
			}
		}


		//
		for ( i = 0; i < editorState.nTextRows; i += 1 )
		{
			// Get next row
			rowIdx += editorState.searchDirection;

			// Wrap around
			if ( rowIdx == - 1 )
			{
				rowIdx = editorState.nTextRows - 1;
			}
			else if ( rowIdx == editorState.nTextRows )
			{
				rowIdx = 0;
			}


			// Search row for occurrence
			textRow = editorState.textRows + rowIdx;

			// Find first occurrence in row
			if ( editorState.searchDirection == SEARCHDIR_FORWARD )
			{
				match = strstr( textRow->chars_render, query );
			}
			// Find last occurrence in row
			else
			{
				startPos = textRow->chars_render + textRow->len_render;

				searchLen = textRow->len_render;

				match = reverseStrstr( startPos, searchLen, query );
			}

			//
			if ( match )
			{
				break;
			}
		}
	}


	// If we found a match...
	if ( match )
	{
		// Move cursor to match
		colIdx = match - textRow->chars_render;

		editorState.editCursorCol = convert_render_to_editCursorCol( textRow, colIdx );
		editorState.editCursorRow = rowIdx;


		// Save match location
		editorState.lastMatchRowIdx = rowIdx;
		editorState.lastMatchColIdx = colIdx;
		editorState.lastMatchLen    = queryLen;


		/* Trigger horizontal scroll to ensure full match is visible,
		   not just first char...
		*/
		editorState.editCursorCol += queryLen;
		scroll();
		editorState.editCursorCol -= queryLen;
	}
}

void find ( void )
{
	char* query;
	int   savedEditCursorRow;
	int   savedEditCursorCol;
	int   savedTextRowOffset;
	int   savedTextColOffset;

	// Save cursor and scroll position
	savedEditCursorRow = editorState.editCursorRow;
	savedEditCursorCol = editorState.editCursorCol;
	savedTextRowOffset = editorState.textRowOffset;
	savedTextColOffset = editorState.textColOffset;


	// Start search
	query = promptUser( "Search: %s", findCallback );


	// User 'escaped' input
	if ( query == NULL )
	{
		// Restore cursor and scroll position
		editorState.editCursorRow = savedEditCursorRow;
		editorState.editCursorCol = savedEditCursorCol;
		editorState.textRowOffset = savedTextRowOffset;
		editorState.textColOffset = savedTextColOffset;
	}
	else
	{
		free( query );
	}
}


// ____________________________________________________________________________________

char* textRowsToString ( int* bufLen )
{
	struct _textRow* textRow;
	int              len;
	int              i;
	char*            buf;
	char*            p;

	// Determine total length
	len = 0;

	for ( i = 0; i < editorState.nTextRows; i += 1 )
	{
		textRow = editorState.textRows + i;

		len += textRow->len + 1;  // +1 for newline
	}

	*bufLen = len;


	// Create and fill buffer
	buf = ( char* ) malloc( len );

	p = buf;

	for ( i = 0; i < editorState.nTextRows; i += 1 )
	{
		textRow = editorState.textRows + i;

		memcpy( p, textRow->chars, textRow->len );

		p += textRow->len;

		*p = '\n';

		p += 1;
	}

	return buf;
}


void save ( void )
{
	char* filename;
	char* buf;
	int   bufLen;
	int   fd;
	int   nWritten;
	char* msg;

	if ( editorState.filename == NULL )
	{
		// TODO: Check that valid filename before assigning...
		filename = promptUser( "Save as: %s", NULL );

		// User 'escaped' input
		if ( filename == NULL )
		{
			return;
		}

		editorState.filename = filename;
	}

	buf = textRowsToString( &bufLen );

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
		nWritten = write( fd, buf, bufLen );

		if ( nWritten != bufLen )
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

void saveAs ( void )
{
	char* filename;

	// TODO: Check that valid filename before assigning...
	filename = promptUser( "Save as: %s", NULL );

	// User 'escaped' input
	if ( filename == NULL )
	{
		return;
	}

	editorState.filename = filename;


	save();
}


// ____________________________________________________________________________________

void freeTextRow ( struct _textRow* textRow )
{
	free( textRow->chars );
	free( textRow->chars_render );
}

void deleteTextRow ( int idx )
{
	struct _textRow* textRow;

	// Check idx within bounds
	if ( ( idx < 0 ) || ( idx > editorState.nTextRows ) )
	{
		return;
	}

	textRow = editorState.textRows + idx;

	freeTextRow( textRow );


	//
	memmove(

		textRow,
		textRow + 1,
		sizeof( struct _textRow ) * ( editorState.nTextRows - idx - 1 )
	);


	// Update
	editorState.nTextRows -= 1;

	// Mark dirty
	editorState.dirty += 1;
}

// Appends a string to the end of a text row
void appendStringToTextRow ( struct _textRow* textRow, char* s, int slen )
{
	char* ptr;

	// Grow to fit string
	ptr = realloc( textRow->chars, textRow->len + slen + 1 );

	if ( ptr == NULL )
	{
		die( "realloc" );
	}

	textRow->chars = ptr;


	// Copy string to end
	memcpy( textRow->chars + textRow->len, s, slen );

	textRow->len += slen;

	textRow->chars[ textRow->len ] = 0;  // null terminate


	// Update
	updateTextRowRender( textRow );

	// Mark dirty
	editorState.dirty += 1;
}


// ____________________________________________________________________________________

void deleteCharFromTextRow ( struct _textRow* textRow, int idx )
{
	// Check idx within bounds
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
	struct _textRow* prevTextRow;

	// If at start of file, nothing to delete
	if ( ( editorState.editCursorRow == 0 ) && ( editorState.editCursorCol == 0 ) )
	{
		return;
	}

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

		// Mark dirty
		editorState.dirty += 1;
	}

	/* Else, delete the "implicit newline" between current line and the
	   one above, merging the two...
	*/
	else
	{
		prevTextRow = curTextRow - 1;

		editorState.editCursorCol = prevTextRow->len;

		appendStringToTextRow( prevTextRow, curTextRow->chars, curTextRow->len );

		deleteTextRow( editorState.editCursorRow );

		editorState.editCursorRow -= 1;
	}
}


// ____________________________________________________________________________________

void insertNewline ( void )
{
	struct _textRow* curTextRow;

	// If cursor is at the start of the line, insert a new row above
	if ( editorState.editCursorCol == 0 )
	{
		insertTextRow( editorState.editCursorRow, "", 0 );
	}

	// Else, split the current row
	else
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;

		// Content of new line is chars to the right of the editCursor
		insertTextRow(

			editorState.editCursorRow + 1,
			curTextRow->chars + editorState.editCursorCol,
			curTextRow->len - editorState.editCursorCol
		);


		/* Reassign pointer because 'realloc' call in 'insertTextRow'
		   might have invalidated our current pointer
		*/
		curTextRow = editorState.textRows + editorState.editCursorRow;

		// Truncate contents of current line
		curTextRow->len = editorState.editCursorCol;

		curTextRow->chars[ curTextRow->len ] = 0;  // null terminate

		updateTextRowRender( curTextRow );
	}

	// Update pos
	editorState.editCursorRow += 1;
	editorState.editCursorCol  = 0;
}


// ____________________________________________________________________________________

void insertCharIntoTextRow ( struct _textRow* textRow, int idx, uchar c )
{
	// Check idx within bounds
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

void insertChar ( uchar c )
{
	struct _textRow* curTextRow;

	/* If cursor is at end of file, append a new text row before
	   we begin inserting a character
	*/
	if ( editorState.editCursorRow == editorState.nTextRows )
	{
		insertTextRow( editorState.nTextRows, "", 0 );
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

void insertTextRow ( int idx, char* s, int slen )
{
	struct _textRow* ptr;
	struct _textRow* textRow;

	// Check idx within bounds
	if ( ( idx < 0 ) || ( idx > editorState.nTextRows ) )
	{
		return;
	}


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


	// Move textRows currently at idx..nTextRows, one row over to idx+1..nTextRows+1
	textRow = editorState.textRows + idx;

	memmove(

		textRow + 1,
		textRow,
		sizeof( struct _textRow ) * ( editorState.nTextRows - idx )
	);


	// Copy the string into the textRow
	textRow->chars = malloc( slen + 1 );  // +1 for null temrinal

	memcpy( textRow->chars, s, slen );

	textRow->chars[ slen ] = 0;  // null terminate

	textRow->len = slen;


	//
	textRow->chars_render = NULL;
	textRow->len_render   = 0;
	updateTextRowRender( textRow );


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
		insertTextRow( editorState.nTextRows, line, lineLen );
	}


	// 'insertTextRow' sets dirty flag, clear it
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

int convert_render_to_editCursorCol ( struct _textRow* textRow, int target_renderCursorCol )
{
	int _editCursorCol;
	int _renderCursorCol;
	int nLeft;
	int nRight;

	_renderCursorCol = 0;

	for ( _editCursorCol = 0; _editCursorCol < textRow->len; _editCursorCol += 1 )
	{
		//
		if ( textRow->chars[ _editCursorCol ] == '\t' )
		{
			// Number of columns we are to the right of the previous tab stop...
			nRight = _renderCursorCol % TABSIZE;

			// Number of columns we are to the left of the next tab stop...
			nLeft = ( TABSIZE - 1 ) - nRight;

			//
			_renderCursorCol += nLeft;
		}

		// Stop when reach target...
		if ( _renderCursorCol == target_renderCursorCol )
		{
			break;
		}

		//
		_renderCursorCol += 1;
	}

	return _editCursorCol;
}


// ____________________________________________________________________________________

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

	// Non-empty line
	if ( editorState.editCursorRow < editorState.nTextRows )
	{
		curTextRow = editorState.textRows + editorState.editCursorRow;

		// If past end of line, snap back to EOL
		if ( editorState.editCursorCol > curTextRow->len )
		{
			editorState.editCursorCol = curTextRow->len;
		}
	}

	// If empty line, snap back to start
	else
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
				setMessage( "Unsaved changes. <ctrl-q> again to quit" );
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

		case CTRL( 't' ):

			saveAs();
			break;

		case CTRL( 'g' ):  // ctrl-h clashing with backspace

			drawHelpScreen();
			break;


		case CTRL( 'f' ):

			find();
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

			insertNewline();
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

			// Do nothing...
			break;


		default:

			insertChar( c );
			break;
	}


	//
	editorState.promptQuit = 1;
}


// ____________________________________________________________________________________

/* ...

   'prompt' is expected to be a format string containing a %s,
   which is where the user's input will be displayed
*/
char* promptUser (

	char* prompt,
	void ( *callback ) ( char*, int, uchar )
)
{
	char*  inputBuf;
	char*  ptr;
	int    inputBufSize;  // size of buffer
	int    inputLen;      // length of input given by user
	uchar  c;

	//
	inputBufSize = 128;  // arbitrary size

	inputBuf = malloc( inputBufSize );

	inputBuf[ 0 ] = 0;


	// Gather user input
	inputLen = 0;

	while ( 1 )
	{
		// Prompt user
		setMessage( prompt, inputBuf );
		editorState.messageIsPrompt = 1;

		refreshScreen();


		// Wait for user input
		c = ( uchar ) readKey();


		// User has pressed escape
		if ( c == K_ESCAPE )
		{
			setMessage( "" );  // clear

			if ( callback != NULL )
			{
				callback( inputBuf, inputLen, c );
			}

			free( inputBuf );

			return NULL;
		}

		// User has pressed enter and their input is not empty
		if ( c == '\n' )
		{
			if ( inputLen > 0 )
			{
				setMessage( "" );  // clear

				if ( callback != NULL )
				{
					callback( inputBuf, inputLen, c );
				}

				return inputBuf;
			}
		}

		// User has pressed backspace
		else if ( ( c == K_BACKSPACE ) || ( c == KEY_DELETE ) )
		{
			// Truncate by one char
			if ( inputLen != 0 )
			{
				inputLen -= 1;

				inputBuf[ inputLen ] = 0;
			}
		}

		// User has entered a printable character, append it to the buffer
		else if ( ( c >= 32 ) && ( c <= 126 ) )
		{
			// Grow buffer if need to...
			if ( inputLen == inputBufSize - 1 )
			{
				inputBufSize *= 2;

				ptr = realloc( inputBuf, inputBufSize );

				if ( ptr == NULL )
				{
					die( "realloc" );
				}

				inputBuf = ptr;
			}

			inputBuf[ inputLen ] = c;

			inputLen += 1;

			inputBuf[ inputLen ] = 0;  // null terminate
		}


		// Call callback after each keypress
		// if ( inputLen > 0 )
		if ( ( ( c >= 32 ) && ( c <= 126 ) ) ||
		     ( c == KEY_LEFT )               ||
		     ( c == KEY_RIGHT )              ||
		     ( c == KEY_UP )                 ||
		     ( c == KEY_DOWN ) )
		{
			if ( callback != NULL )
			{
				callback( inputBuf, inputLen, c );
			}
		}
	}
}

void setMessage ( const char* fmt, ... )
{
	va_list argp;

	//
	va_start( argp, fmt );

	vsnprintf( editorState.message, STATUSBUFSZ, fmt, argp );

	va_end( argp );


	//
	editorState.messageTime = uptime();  // "ticks" since kernel started...
	// editor.messageTime = time();      // seconds since epoch


	// Hmmm... for now set default here
	editorState.messageIsWarning = 0;
	editorState.messageIsPrompt  = 0;
}


// ____________________________________________________________________________________

void drawHelpScreen ( void )
{
	uchar c;
	int   i;

	// Draw message
	GFXText_clearScreen();

	for ( i = 0; i < helpMsgLen; i += 1 )
	{
		GFXText_setCursorPosition( i, 0 );

		printString( helpMsg[ i ], NO_WRAP );
	}


	// Block until user presses escape
	while ( 1 )
	{
		// Wait for user input
		c = ( uchar ) readKey();


		// User has pressed escape
		if ( c == K_ESCAPE )
		{
			return;
		}
	}
}


// ____________________________________________________________________________________

void drawRows ( void )
{
	int              screenRow;
	int              fileRow;
	struct _textRow* textRow;
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
				textRow = editorState.textRows + fileRow;

				/* If haven't scrolled horizontally past end of line,
				   draw the visible part...
				*/
				if ( editorState.textColOffset < textRow->len_render )
				{
					text = textRow->chars_render + editorState.textColOffset;

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
		if ( editorState.messageIsWarning || editorState.messageIsPrompt )
		{
			curTextColor   = GFXText_getTextColor(); 
			curTextBgColor = GFXText_getTextBgColor();

			if ( editorState.messageIsWarning )
			{
				GFXText_setTextColor( warningTextColor );
				GFXText_setTextBgColor( warningTextBgColor );
			}
			else
			{
				GFXText_setTextColor( promptTextColor );
				GFXText_setTextBgColor( promptTextBgColor );
			}
		}


		// Draw message
		GFXText_setCursorPosition( editorState.nScreenRows + 1, 0 );

		printString( editorState.message, NO_WRAP );


		// Restore colors
		if ( editorState.messageIsWarning || editorState.messageIsPrompt )
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


	//
	editorState.lastMatchRowIdx = - 1;
	editorState.searchDirection = SEARCHDIR_FORWARD;
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

	setMessage( "<ctrl-q> quit, <ctrl-g> help" );

	while ( 1 )
	{
		refreshScreen();

		processKeyPress();  // atm, blocks
	}

	cleanup();

	exit();
}
