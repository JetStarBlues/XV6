#include "types.h"
#include "user.h"
#include "GFXtext.h"

#define UINPUTBUFSZ 3

#define NROWS 18
#define NCOLS 40

int main ( int argc, char* argv [] )
{
	char  uinputbuf [ UINPUTBUFSZ ];

	uint i;
	uint row;
	uint col;
	uint c;

	initGFXText();

	// setTextBgColor( 64 );
	// clearScreen();


	//
	setTextBgColor( 3 );
	setCursorPosition( 0, 0 );
	printChar( 'H' );
	setCursorPosition( 0, 1 );
	printChar( 'e' );
	setCursorPosition( 0, 2 );
	printChar( 'l' );
	setCursorPosition( 0, 3 );
	printChar( 'l' );
	setCursorPosition( 0, 4 );
	printChar( 'o' );


	//
	i = 0;
	for ( c = 0; c <= 255; c += 1 )
	{
		row = i / NCOLS;
		col = i % NCOLS;

		setCursorPosition( 2 + row, col );
		printChar( ( uchar ) c );

		i += 1;
	}


	// cursor...
	setCursorPosition( 0, 2 );
	drawCursor();



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

	exitGFXText();

	exit();
}
