/*
  Based on this excellent tutorial:
    "Build your own text editor"
    by Jeremy Ruten
    https://viewsourcecode.org/snaptoken/kilo/index.html
*/

/* NOTE: QEMU window has to be in focus for
         navigation keys to work (UP/DOWN/HOME etc)
*/


/*
TODO:
    . Text wrapping

    . Mouse select ?
       . Plus thing where auto scrolls

    . Copy and paste
*/


#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "kernel/termios.h"
#include "user.h"
#include "GFX.h"      // Render graphically
#include "GFXtext.h"  // Render graphically


// -- User configurable start ----------------------------------

// Default colors (standard 256-color VGA palette)
static uchar textColor          = 0x18;  // grey
static uchar textBgColor        = 0x0F;  // white
static uchar cursorColor        = 0x70;  // brown
static uchar warningTextColor   = 0x0F;  // white
static uchar warningTextBgColor = 0x0C;  // red
static uchar promptTextColor    = 0x18;  // grey
static uchar promptTextBgColor  = 0x5C;  // light yellow

static uchar hlColor_searchResult      = 0x09;  // blue
static uchar hlColor_number            = 0x26;  // pink
static uchar hlColor_string            = 0x2F;  // green
static uchar hlColor_commentSingleLine = 0x1C;  // lighter grey
static uchar hlColor_commentMultiLine  = 0x1C;  // lighter grey
static uchar hlColor_keywords0         = 0x3A;  // baby purple
static uchar hlColor_keywords1         = 0x4F;  // baby blue
static uchar hlColor_operators         = 0x2B;  // orange


//
#define TABSIZE 3   // tab size in spaces


// -- User configurable end ------------------------------------




// -------------------------------------------------------------

// Syntax highlighting
#define HL_NORMAL             1
#define HL_SEARCHMATCH        2
#define HL_NUMBER             3
#define HL_STRING             4
#define HL_COMMENT_SINGLElINE 5
#define HL_COMMENT_MULTILINE  6
#define HL_OPERATORS          7
#define HL_KEYWORDS0          8
#define HL_KEYWORDS1          9

struct _syntax {

	char*  syntaxName;
	char** fileExtensions;  // list of file extensions that use this syntax

	char*  singleLineCommentStart;
	int    singleLineCommentStartLen;  // excluding null terminal
	char*  multiLineCommentStart;
	int    multiLineCommentStartLen;   // excluding null terminal
	char*  multiLineCommentEnd;
	int    multiLineCommentEndLen;     // excluding null terminal

	char** keywords0;
	char** keywords1;

	int ( *isOperatorChar  ) ( int );
	int ( *isSeparatorChar ) ( int );
};


// -------------------------------------------------------------

/* For now using fixed value. Ideally would use runtime value of
   'editorState.nScreenCols + 1'
*/
#define STATUSBUFSZ 81


// -------------------------------------------------------------

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


// -------------------------------------------------------------

//
struct _textRow {

	int   idx;    // track position in file

	char* chars;  // null terminated
	int   len;    // excluding null-terminal

	/* What rendered might be different length than bytes on file.
	   For example tabs.
	*/
	char* chars_render;  // null terminated
	int   len_render;    // excluding null-terminal

	/* Store byte of highlight info for each byte of chars_render
	*/
	char* highlightInfo;
	int   inMultilineComment;  // inside unclosed multiline comment
};


//
#define SEARCHDIR_FORWARD      1    // increment
#define SEARCHDIR_BACKWARD ( - 1 )  // decrement

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
	struct _textRow* textRows;
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
	int dirty;         // unsaved changes
	int promptQuit;    // prompt on quit with unsaved changes
	int promptClose0;  // prompt on close with unsaved changes
	int promptClose1;  // prompt on close with unsaved changes

	//
	int   searchDirection;
	int   lastMatchRowIdx;       // -1 if no last match
	int   lastMatchColIdx;
	int   lastMatchLen;
	char* savedHighlightInfo;    // restore after exit search
	int   savedHighlightRowIdx;

	//
	struct _syntax* currentSyntax;  // specifies which (if any) syntax should be used for highlighting
};

static struct _editorState editorState;


// -------------------------------------------------------------

// Help message
static char* helpMsg [] = {

	"-- Shortcuts --",
	"",
	". [ctrl-g]     view help (this page)",
	"    . [esc]       cancel",
	". [ctrl-s]     save",
	"    . [esc]       cancel",
	". [ctrl-t]     save as",
	"    . [esc]       cancel",
	"    . [enter]     accept",
	". [ctrl-n]     open a new file",
	". [ctrl-o]     open an existing file",
	"    . [esc]       cancel",
	"    . [enter]     accept",
	". [ctrl-f]     find",
	"    . [esc]       cancel",
	"    . [arrow]     jump to next/prev match",
	"    . [enter]     accept",
	". [ctrl-q]     quit",
};

static int helpMsgLen = sizeof( helpMsg ) / sizeof( char* );


// ____________________________________________________________________________________

/* Syntax highlighting rules.
   Ideally these would be in separate files (one per language).
*/

// ------------------------------------------------------------------------------------
// C Language
// ------------------------------------------------------------------------------------

char* CLikeFileExtensions [] = {

	".c", ".h", ".cpp", NULL
};

char* CLikeKeywords0 [] = {

	"void",
	"char", "int", "long",
	"float", "double",
	"unsigned", "signed",
	"static", "const",

	"union", "struct", "enum",
	"typedef",
	NULL
};

char* CLikeKeywords1 [] = {

	"for", "while", "break", "continue",
	"if", "then", "else",
	"return",
	"switch", "case", "default", "goto",

	"#include", "#define",
	NULL
};

/*char* CLikeOperators [] = {

	"=",
	"+", "-", "/", "%", "*",
	"~", "&", "|", "^",
	"&&", "||", "!",
	"!=", "==", ">=", "<=", ">", "<",
	NULL
};*/

int CLike_isOperatorChar ( int c )
{
	return ( strchr( "+-/*%~&|^=<>", c ) != NULL );
}

int CLike_isSeparatorChar ( int c )
{
	return (

		/* Given that we store textRows as null delimited array,
		   detects end of textRow */
		c == 0                                  ||

		// Whitespace
		ISBLANK( c )                            ||

		// Delimiter char
		( strchr( ";,.()[]", c ) != NULL )      ||

		// Operator char
		( strchr( "+-/*%~&|^=<>", c ) != NULL )
	);
}


struct _syntax CLikeSyntax = {

	"C",
	CLikeFileExtensions,

	"//", 2,
	"/*", 2,
	"*/", 2,

	CLikeKeywords0,
	CLikeKeywords1,

	CLike_isOperatorChar,
	CLike_isSeparatorChar
};


// ------------------------------------------------------------------------------------
// Available to use
// ------------------------------------------------------------------------------------

struct _syntax* syntaxDatabase [] = {

	&CLikeSyntax
};

static int syntaxDatabaseLen = sizeof( syntaxDatabase ) / sizeof( struct _syntax* );


// ____________________________________________________________________________________

// Forward declarations...
uchar getHighlightColor               ( int );
void  updateRowSyntaxHighlight        ( struct _textRow* );
int   convert_render_to_editCursorCol ( struct _textRow*, int );
void  freeTextRow                     ( struct _textRow* );
void  deleteTextRow                   ( int );
void  insertTextRow                   ( int, char*, int );
void  scroll                          ( void );

char* promptUser (
	char*,
	void ( * ) ( char*, int, uchar )
);

void  setMessage                      ( const char*, ... );
void  drawLoadingFileScreen           ( void );
void  drawHelpScreen                  ( void );
void  refreshScreen                   ( void );
void  initEditor                      ( void );
void  die                             ( char* );



// =========================================================================================

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
	newConsoleAttr.icanon = 0;  // disable canonical mode input

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



// =========================================================================================

/* We are responsible for cursor movement...
*/

#define WRAP    0
#define NO_WRAP 1

/* TODO - If performance is a problem, can reduce per character
   calls to 'GFXText_setTextColor' and co. by tracking
   current color and changing only if different.
*/
void printString ( char* s, int wrap, char* hlInfo )
{
	int   row;
	int   col;
	int   i;
	uchar curTextColor;
	char  hlSyntax;
	uchar hlSyntaxColor;

	//
	if ( wrap == WRAP )
	{
		die( "printString: text wrapping currently unsupported" );
	}

	// Save
	if ( hlInfo != NULL )
	{
		curTextColor = GFXText_getTextColor();
	}

	// Get current position
	GFXText_getCursorPosition( &row, &col );

	//
	hlSyntax      = 0;
	hlSyntaxColor = 0;
	i             = 0;

	while ( *s )
	{
		// Set color
		if ( hlInfo != NULL )
		{
			hlSyntax      = hlInfo[ i ];
			hlSyntaxColor = getHighlightColor( hlSyntax );

			GFXText_setTextColor( hlSyntaxColor );

			// Draw with different bg color so stands out...
			if ( hlSyntax == HL_SEARCHMATCH )
			{
				GFXText_invertTextColors();
			}
		}


		//
		GFXText_printChar( *s );
		// printf( stdout, "%c", *s );


		// Advance cursor
		col += 1;

		// Handle horizontal overflow
		if ( col == editorState.nScreenCols )
		{
			// If wrapping enabled, draw overflow characters in newline
			/* TODO: for this to properly work, would need a "render cursor"
			   or somehow update/track in 'chars_render' such that editor logic
			   is aware of screen location of character...
			*/
			/*
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

			// Otherwise, don't draw overflow characters
			else
			{
				break;
			}
			*/

			// Before we break, restore...
			if ( ( hlInfo != NULL ) && ( hlSyntax == HL_SEARCHMATCH ) )
			{
				GFXText_invertTextColors();
			}

			//
			break;
		}

		GFXText_setCursorPosition( row, col );


		// Before we draw next char, restore...
		if ( ( hlInfo != NULL ) && ( hlSyntax == HL_SEARCHMATCH ) )
		{
			GFXText_invertTextColors();
		}


		// Evaluate next char
		s += 1;
		i += 1;
	}


	// Restore
	if ( hlInfo != NULL )
	{
		GFXText_setTextColor( curTextColor );
	}
}



// =========================================================================================

void setFileSyntaxHighlight ( void )
{
	char*            fileExtension;
	uint             i;
	uint             j;
	struct _syntax*  syntax;
	char*            syntaxExtension;
	int              rowIdx;
	struct _textRow* textRow;

	// Default, no syntax highlighting
	editorState.currentSyntax = NULL;

	if ( editorState.filename == NULL )
	{
		return;
	}


	// Get file's extension
	fileExtension = strrchr( editorState.filename, '.' );


	/* If file has an extension, loop through database
	   looking for a match.
	*/
	if ( fileExtension )
	{
		for ( j = 0; j < syntaxDatabaseLen; j += 1 )
		{
			syntax = syntaxDatabase[ j ];

			i = 0;

			while ( 1 )
			{
				syntaxExtension = syntax->fileExtensions[ i ];

				// Reached end of list
				if ( syntaxExtension == NULL )
				{
					break;  // evaluate next entry in database
				}

				// Found a match!
				if ( strcmp( fileExtension, syntaxExtension ) == 0 )
				{
					//
					editorState.currentSyntax = syntax;

					// Re-highlight file. For example when filename changes.
					for ( rowIdx = 0; rowIdx < editorState.nTextRows; rowIdx += 1 )
					{
						textRow = editorState.textRows + rowIdx;

						updateRowSyntaxHighlight( textRow );
					}

					// Done
					return;
				}

				i += 1;
			}
		}
	}
}


// ____________________________________________________________________________________

uchar getHighlightColor ( int snytaxType )
{
	switch ( snytaxType )
	{
		case HL_SEARCHMATCH        : return hlColor_searchResult;
		case HL_NUMBER             : return hlColor_number;
		case HL_STRING             : return hlColor_string;
		case HL_COMMENT_SINGLElINE : return hlColor_commentSingleLine;
		case HL_COMMENT_MULTILINE  : return hlColor_commentMultiLine;
		case HL_OPERATORS          : return hlColor_operators;
		case HL_KEYWORDS0          : return hlColor_keywords0;
		case HL_KEYWORDS1          : return hlColor_keywords1;
	}

	// Default
	return textColor;
}


// ____________________________________________________________________________________

int highlightCommentSingleLine ( struct _textRow* textRow, int idx, int inString, int inMultilineComment )
{
	char* sCommentMark;
	int   sCommentMarkLen;

	sCommentMark    = editorState.currentSyntax->singleLineCommentStart;
	sCommentMarkLen = editorState.currentSyntax->singleLineCommentStartLen;


	if ( ( sCommentMarkLen > 0 ) && ( ! inString ) && ( ! inMultilineComment ) )
	{
		if ( strncmp( sCommentMark, textRow->chars_render + idx, sCommentMarkLen ) == 0 )
		{
			memset(

				textRow->highlightInfo + idx,
				HL_COMMENT_SINGLElINE,
				textRow->len_render - idx
			);

			return 1;  // Done processing line
		}
	}


	// No match
	return 0;
}

int highlightCommentMultiLine ( struct _textRow* textRow, int* idx, int* prevCharWasSep, char inString, int* inMultilineComment )
{
	char* mCommentStartMark;
	int   mCommentStartMarkLen;
	char* mCommentEndMark;
	int   mCommentEndMarkLen;

	mCommentStartMark    = editorState.currentSyntax->multiLineCommentStart;
	mCommentStartMarkLen = editorState.currentSyntax->multiLineCommentStartLen;
	mCommentEndMark      = editorState.currentSyntax->multiLineCommentEnd;
	mCommentEndMarkLen   = editorState.currentSyntax->multiLineCommentEndLen;


	if ( mCommentStartMarkLen && mCommentEndMarkLen && ( ! inString ) )
	{
		//
		if ( *inMultilineComment )
		{
			// Matched end of multiline comment
			if ( strncmp( mCommentEndMark, textRow->chars_render + ( *idx ), mCommentEndMarkLen ) == 0 )
			{
				memset(

					textRow->highlightInfo + ( *idx ),
					HL_COMMENT_MULTILINE,
					mCommentEndMarkLen
				);

				*idx += mCommentEndMarkLen;  // consume

				*inMultilineComment = 0;

				*prevCharWasSep = 1;

				return 1;
			}

			//
			else
			{
				textRow->highlightInfo[ *idx ] = HL_COMMENT_MULTILINE;

				*idx += 1;  // consume

				return 1;
			}
		}

		// Matched start of multiline comment
		else if ( strncmp( mCommentStartMark, textRow->chars_render + ( *idx ), mCommentStartMarkLen ) == 0 )
		{
			memset(

				textRow->highlightInfo + ( *idx ),
				HL_COMMENT_MULTILINE,
				mCommentStartMarkLen
			);

			*idx += mCommentStartMarkLen;  // consume

			*inMultilineComment = 1;

			return 1;
		}
	}


	// No match
	return 0;
}

int highlightString ( struct _textRow* textRow, int* idx, int* prevCharWasSep, char* inString )
{
	char c;

	c = textRow->chars_render[ *idx ];

	if ( *inString )
	{
		//
		textRow->highlightInfo[ *idx ] = HL_STRING;

		// Check for escaped characters
		if ( ( c == '\\' ) && ( ( *idx + 1 ) < textRow->len_render ) )
		{
			textRow->highlightInfo[ *idx + 1 ] = HL_STRING;

			*idx += 2;  // consume

			return 1;
		}

		// Reached closing quote
		if ( c == ( *inString ) )
		{
			*inString = 0;

			*prevCharWasSep = 1;  // Closing quote treated as separator
		}

		*idx += 1;  // consume

		return 1;
	}

	else
	{
		if ( ( c == '"' ) || ( c == '\'' ) )
		{
			*inString = c;  // mark with quote type

			textRow->highlightInfo[ *idx ] = HL_STRING;

			*idx += 1;  // consume

			return 1;
		}
	}


	// No match
	return 0;
}

int highlightOperator ( struct _textRow* textRow, int* idx, int* prevCharWasSep )
{
	char c;

	c = textRow->chars_render[ *idx ];

	if ( editorState.currentSyntax->isOperatorChar( c ) )
	{
		textRow->highlightInfo[ *idx ] = HL_OPERATORS;

		*idx += 1;  // consume

		*prevCharWasSep = 1;

		return 1;
	}


	// No match
	return 0;
}

int highlightNumber ( struct _textRow* textRow, int* idx, int* prevCharWasSep )
{
	char c;
	char prevHlSyntax;

	// Get highlight type of previous character
	if ( *idx > 0 )
	{
		prevHlSyntax = textRow->highlightInfo[ *idx - 1 ];
	}
	else
	{
		prevHlSyntax = HL_NORMAL;
	}

	//
	c = textRow->chars_render[ *idx ];

	//
	if (
		/* Current char is a digit, and previous char was either
		   a separator or a number
		*/
		(
			ISDIGIT( c ) &&
			( *prevCharWasSep || ( prevHlSyntax == HL_NUMBER ) )
		)

		||

		// Current char is a dot, and previous char was a number
		(
			( c == '.' ) && ( prevHlSyntax == HL_NUMBER )
		)
	)
	{
		textRow->highlightInfo[ *idx ] = HL_NUMBER;

		*idx += 1;  // consume

		*prevCharWasSep = 0;

		return 1;
	}


	// No match
	return 0;
}

int highlightKeyword ( struct _textRow* textRow, int* idx, int* prevCharWasSep, char** keywords, char hlSyntax )
{
	int   i;
	char* keyword;
	int   keywordLen;

	if ( *prevCharWasSep )  // require a separator before the keyword
	{
		for ( i = 0; keywords[ i ]; i += 1 )
		{
			keyword = keywords[ i ];

			keywordLen = strlen( keyword );

			if (

				// keyword matches
				( strncmp( keyword, textRow->chars_render + ( *idx ), keywordLen ) == 0 )

				&&

				// require a separator after the keyword
				( editorState.currentSyntax->isSeparatorChar(

					*( textRow->chars_render + ( *idx ) + keywordLen )
				) )
			)
			{
				memset(

					textRow->highlightInfo + ( *idx ),
					hlSyntax,
					keywordLen
				);

				*idx += keywordLen;  // consume

				*prevCharWasSep = 0;  // ?

				return 1;
			}
		}
	}


	// No keyword matched
	return 0;
}


void updateRowSyntaxHighlight ( struct _textRow* textRow )
{
	struct _textRow* textRowPrev;
	struct _textRow* textRowNext;
	char             c;
	int              i;

	int              mCommentStatusChanged;
	char**           keywords;

	int              matchedCommentSingle;
	int              matchedCommentMultiline;
	int              matchedString;
	int              matchedOperator;
	int              matchedNumber;
	int              matchedKeyword;

	int              prevCharWasSep;
	char             inString;
	int              inMultilineComment;


	/* Since overwriting everything, less overhead to
	   free than realloc.
	*/
	free( textRow->highlightInfo );

	textRow->highlightInfo = malloc( textRow->len_render );


	// Default
	memset( textRow->highlightInfo, HL_NORMAL, textRow->len_render );


	//
	if ( editorState.currentSyntax == NULL )
	{
		return;
	}


	//
	prevCharWasSep     = 1;  // Beginning of line treated as separator
	inString           = 0;
	inMultilineComment = 0;

	// Check if previous line is part of an unclosed multiline comment
	if ( textRow->idx > 0 )
	{
		textRowPrev = editorState.textRows + textRow->idx - 1;

		if ( textRowPrev->inMultilineComment )
		{
			inMultilineComment = 1;
		}
	}


	i = 0;

	while ( i < textRow->len_render )
	{
		// Match single line comments
		matchedCommentSingle = highlightCommentSingleLine( textRow, i, inString, inMultilineComment );

		if ( matchedCommentSingle )
		{
			break;  // Done processing line
		}


		// Match multiline comment
		matchedCommentMultiline = highlightCommentMultiLine( textRow, &i, &prevCharWasSep, inString, &inMultilineComment );

		if ( matchedCommentMultiline )
		{
			continue;
		}


		// Match string
		matchedString = highlightString( textRow, &i, &prevCharWasSep, &inString );

		if ( matchedString )
		{
			continue;
		}


		// Match operator
		matchedOperator = highlightOperator( textRow, &i, &prevCharWasSep );

		if ( matchedOperator )
		{
			continue;
		}


		// Match number
		matchedNumber = highlightNumber( textRow, &i, &prevCharWasSep );

		if ( matchedNumber )
		{
			continue;
		}


		// Match keyword
		{
			// Check group 0
			keywords = editorState.currentSyntax->keywords0;

			matchedKeyword = highlightKeyword( textRow, &i, &prevCharWasSep, keywords, HL_KEYWORDS0 );

			if ( matchedKeyword )
			{
				continue;
			}


			// Check group 1
			keywords = editorState.currentSyntax->keywords1;

			matchedKeyword = highlightKeyword( textRow, &i, &prevCharWasSep, keywords, HL_KEYWORDS1 );

			if ( matchedKeyword )
			{
				continue;
			}
		}


		//
		c = textRow->chars_render[ i ];

		prevCharWasSep = editorState.currentSyntax->isSeparatorChar( c );


		//
		i += 1;
	}


	// Update current row's multiline comment state (inside/outside)
	mCommentStatusChanged = textRow->inMultilineComment != inMultilineComment;

	textRow->inMultilineComment = inMultilineComment;

	// Update subsequent rows affected by change in multiline comment state
	if ( mCommentStatusChanged && ( ( textRow->idx + 1 ) < editorState.nTextRows ) )
	{
		textRowNext = editorState.textRows + textRow->idx + 1;

		updateRowSyntaxHighlight( textRowNext );
	}
}



// =========================================================================================

void openFile ( char* filename )
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


	// Give the user some hope
	drawLoadingFileScreen();


	//
	editorState.filename = strdup( filename );

	//
	setFileSyntaxHighlight();


	//
	line        = NULL;
	lineLen     = 0;
	lineBufSize = 0;

	while ( 1 )
	{
		// Use 'getline' to read a line from the file
		lineLen = getline( &line, &lineBufSize, fd );
		// printf( stdout, "getline returned: (%d) %s\n", lineLen, line );

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

void closeFile ( void )
{
	struct _textRow* textRow;
	int              i;

	// If empty file, nothing to do
	if ( ( editorState.filename == NULL ) && ( editorState.dirty == 0 ) )
	{
		return;
	}


	// Free memory currently in use
	for ( i = 0; i < editorState.nTextRows; i += 1 )
	{
		textRow = editorState.textRows + i;

		freeTextRow( textRow );
	}

	free( editorState.textRows );	
	free( editorState.filename );
	free( editorState.savedHighlightInfo );


	// Reset editor state
	initEditor();
}

// Open new empty file
void openNewFile ( void )
{
	// Close current file
	closeFile();
}

// Open file specified by user
void openExistingFile ( void )
{
	char* filename;

	// TODO: Check that valid filename before assigning...
	filename = promptUser( "Open: %s", NULL );

	// User 'escaped' input
	if ( filename == NULL )
	{
		return;
	}


	// Close current file
	closeFile();

	// Open new file
	openFile( filename );
}



// =========================================================================================

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

void saveFile ( void )
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

		// Update to reflect new filename
		setFileSyntaxHighlight();
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

void saveFileAs ( void )
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

	// Update to reflect new filename
	setFileSyntaxHighlight();


	saveFile();
}



// =========================================================================================

/* Returns a pointer to the beginning of the last occurrence
   of substring 'sub' in string 'sEnd - searchLen'.
   If not found, returns NULl.
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


	// Restore colors of line containing previous match
	if ( editorState.savedHighlightInfo != NULL )
	{
		textRow = editorState.textRows + editorState.savedHighlightRowIdx;

		memcpy(

			textRow->highlightInfo,
			editorState.savedHighlightInfo,
			textRow->len_render
		);

		free( editorState.savedHighlightInfo );

		editorState.savedHighlightInfo = NULL;
	}


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


	/* If user has left search mode, return early
	   (after above free and resets)
	*/
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


		// Save line's current colors
		editorState.savedHighlightRowIdx = rowIdx;
		editorState.savedHighlightInfo = malloc( textRow->len_render );
		memcpy(

			editorState.savedHighlightInfo,
			textRow->highlightInfo,
			textRow->len_render
		);


		// Highlight match
		memset( textRow->highlightInfo + colIdx, HL_SEARCHMATCH, queryLen );
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



// =========================================================================================

void updateRowRender ( struct _textRow* textRow )
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


	/* Since overwriting everything, less overhead to
	   free than realloc.
	*/
	free( textRow->chars_render );

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
			// Each tab must advance the cursor forward at least one column
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


	// Update corresponding highlight info
	updateRowSyntaxHighlight( textRow );
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



// =========================================================================================

// Used to process <enter> key
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

		updateRowRender( curTextRow );
	}

	// Update pos
	editorState.editCursorRow += 1;
	editorState.editCursorCol  = 0;
}


// ____________________________________________________________________________________

/* Appends a string to the end of a text row.
   Used to process <backspace/delete> keys
*/
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
	updateRowRender( textRow );

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
	updateRowRender( textRow );
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
	updateRowRender( textRow );
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

void freeTextRow ( struct _textRow* textRow )
{
	free( textRow->chars );
	free( textRow->chars_render );
	free( textRow->highlightInfo );
}

void deleteTextRow ( int idx )
{
	struct _textRow* textRow;
	struct _textRow* textRow2;
	int              i;

	// Check idx within bounds
	if ( ( idx < 0 ) || ( idx > editorState.nTextRows ) )
	{
		return;
	}

	//
	textRow = editorState.textRows + idx;

	// Free associated memory
	freeTextRow( textRow );


	//
	memmove(

		textRow,
		textRow + 1,
		sizeof( struct _textRow ) * ( editorState.nTextRows - idx - 1 )
	);

	// Update indices affected by deletion
	for ( i = idx; i < editorState.nTextRows - 1; i += 1 )
	{
		textRow2 = editorState.textRows + i;

		textRow2->idx -= 1;
	}


	// Update
	editorState.nTextRows -= 1;

	// Mark dirty
	editorState.dirty += 1;
}


void insertTextRow ( int idx, char* s, int slen )
{
	struct _textRow* ptr;
	struct _textRow* textRow;
	struct _textRow* textRow2;
	int              i;

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


	// Update indices affected by insertion
	for ( i = idx + 1; i < editorState.nTextRows; i += 1 )
	{
		textRow2 = editorState.textRows + i;

		textRow2->idx += 1;
	}


	// Copy the string into the textRow
	textRow->chars = malloc( slen + 1 );  // +1 for null terminal

	memcpy( textRow->chars, s, slen );

	textRow->chars[ slen ] = 0;  // null terminate

	textRow->len = slen;


	//
	textRow->idx                = idx;
	textRow->chars_render       = NULL;
	textRow->len_render         = 0;
	textRow->highlightInfo      = NULL;
	textRow->inMultilineComment = 0;


	//
	updateRowRender( textRow );

	// Update count
	editorState.nTextRows += 1;

	// Mark dirty
	editorState.dirty += 1;
}



// =========================================================================================

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



// =========================================================================================

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
				//
				setMessage( "Unsaved changes. [ctrl-q] again to quit" );
				editorState.messageIsWarning = 1;

				// Clear counter
				editorState.promptQuit = 0;

				// Force screen refresh so message appears
				refreshScreen();

				// Reset other counters...
				editorState.promptClose0 = 1;
				editorState.promptClose1 = 1;

				//
				return;
			}
			else
			{
				die( NULL );
				break;
			}

		case CTRL( 'o' ):

			if ( editorState.dirty && editorState.promptClose0 )
			{
				setMessage( "Unsaved changes. [ctrl-o] again to open" );
				editorState.messageIsWarning = 1;

				// Clear counter
				editorState.promptClose0 = 0;

				// Force screen refresh so message appears
				refreshScreen();

				// Reset other counters...
				editorState.promptQuit   = 1;
				editorState.promptClose1 = 1;

				//
				return;
			}
			else
			{
				openExistingFile();
				break;
			}

		case CTRL( 'n' ):

			if ( editorState.dirty && editorState.promptClose1 )
			{
				setMessage( "Unsaved changes. [ctrl-n] again to open" );
				editorState.messageIsWarning = 1;

				// Clear counter
				editorState.promptClose1 = 0;

				// Force screen refresh so message appears
				refreshScreen();

				// Reset other counters...
				editorState.promptQuit   = 1;
				editorState.promptClose0 = 1;

				//
				return;
			}
			else
			{
				openNewFile();
				break;
			}

		case CTRL( 's' ):

			saveFile();
			break;

		case CTRL( 't' ):

			saveFileAs();
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
	editorState.promptQuit   = 1;
	editorState.promptClose0 = 1;
	editorState.promptClose1 = 1;
}



// =========================================================================================

/* 'prompt' is expected to be a format string containing a %s,
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



// =========================================================================================

void drawLoadingFileScreen ( void )
{
	char* msg;
	int   msgLen;
	int   centerRow;
	int   centerCol;

	msg    = "Loading file...";
	msgLen = 15;  // excluding null terminal

	centerRow = editorState.nScreenRows / 2 - 1;  // -1 for aesthetic
	centerCol = ( editorState.nScreenCols / 2 ) - ( msgLen / 2 );

	GFXText_clearScreen();
	GFXText_setCursorPosition( centerRow, centerCol );
	printString( msg, NO_WRAP, NULL );	
}

void drawHelpScreen ( void )
{
	uchar c;
	int   i;

	// Draw message
	GFXText_clearScreen();

	for ( i = 0; i < helpMsgLen; i += 1 )
	{
		GFXText_setCursorPosition( i, 0 );

		printString( helpMsg[ i ], NO_WRAP, NULL );
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

void drawTextRows ( void )
{
	int              screenRow;
	int              fileRow;
	struct _textRow* textRow;
	char*            text;
	char*            hlInfo;
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

				printString( welcomeMsg, NO_WRAP, NULL );
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
	
					hlInfo = textRow->highlightInfo + editorState.textColOffset;

					printString( text, NO_WRAP, hlInfo );
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

	printString( statusMsg, NO_WRAP, NULL );

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
			printString( statusMsg, NO_WRAP, NULL );

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

		printString( editorState.message, NO_WRAP, NULL );


		// Restore colors
		if ( editorState.messageIsWarning || editorState.messageIsPrompt )
		{
			GFXText_setTextColor( curTextColor );
			GFXText_setTextBgColor( curTextBgColor );
		}
	}
}



// =========================================================================================

void refreshScreen ( void )
{
	//
	GFXText_clearScreen();

	//
	scroll();

	//
	drawTextRows();
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
	editorState.dirty        = 0;
	editorState.promptQuit   = 1;
	editorState.promptClose0 = 1;
	editorState.promptClose1 = 1;


	//
	editorState.searchDirection    = SEARCHDIR_FORWARD;
	editorState.lastMatchRowIdx    = - 1;
	editorState.savedHighlightInfo = NULL;

	//
	editorState.currentSyntax = NULL;
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

	setMessage( "[ctrl-q] quit, [ctrl-g] help" );

	while ( 1 )
	{
		refreshScreen();

		processKeyPress();  // atm, blocks
	}

	cleanup();

	exit();
}
