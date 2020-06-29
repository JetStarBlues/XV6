#include "kernel/types.h"
#include "user.h"
#include "GFX.h"
#include "GFXtext.h"

#define UINPUTBUFSZ 3


int main ( int argc, char* argv [] )
{
	char  uinputbuf [ UINPUTBUFSZ ];

	int i;
	int row;
	int col;
	int c;
	int nRows;
	int nCols;

	GFX_init();

	GFXText_setTextBgColor( 64 );
	GFXText_clearScreen();


	//
	GFXText_setTextBgColor( 3 );
	GFXText_setCursorPosition( 0, 0 );
	GFXText_printChar( 'H' );
	GFXText_setCursorPosition( 0, 1 );
	GFXText_printChar( 'e' );
	GFXText_setCursorPosition( 0, 2 );
	GFXText_printChar( 'l' );
	GFXText_setCursorPosition( 0, 3 );
	GFXText_printChar( 'l' );
	GFXText_setCursorPosition( 0, 4 );
	GFXText_printChar( 'o' );


	//
	GFXText_getDimensions( &nRows, &nCols );

	i = 0;
	for ( c = 0; c <= 255; c += 1 )
	{
		row = i / nCols;
		col = i % nCols;

		GFXText_setCursorPosition( 2 + row, col );
		GFXText_printChar( ( uchar ) c );

		i += 1;
	}


	// cursor...
	GFXText_setCursorPosition( 0, 2 );
	GFXText_drawCursor();



	/* When user enters "q", switch back to text mode.
	   Ideally this would be an "onkeypress" event handler;
	   for now we sleep on stdin
	*/
	while ( 1 )
	{
		memset( uinputbuf, 0, UINPUTBUFSZ );

		gets( uinputbuf, UINPUTBUFSZ );

		uinputbuf[ strlen( uinputbuf ) - 1 ] = 0;  // remove newline char '\n'

		if ( strcmp( uinputbuf, "q" ) == 0 )
		{
			break;
		}
	}

	GFX_exit();

	exit();
}
