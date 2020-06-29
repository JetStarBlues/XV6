/* Draws the current VGA palette
*/

#include "kernel/types.h"
#include "kernel/fcntl.h"
#include "user.h"
#include "GFX.h"

#define UINPUTBUFSZ 3

void drawPalette ( void )
{
	int cellWidth;
	int cellHeight;
	int colorIdx;
	int x;
	int y;

	cellWidth  = SCREEN_WIDTH / 16;
	cellHeight = SCREEN_HEIGHT / 16;

	colorIdx = 0;

	for ( y = 0; y < SCREEN_HEIGHT; y += cellHeight )
	{
		for ( x = 0; x < SCREEN_WIDTH; x += cellWidth )
		{
			GFX_fillRect( x, y, cellWidth, cellHeight, colorIdx );

			colorIdx += 1;

			// Early exit...
			if ( colorIdx == 256 )
			{
				return;
			}
		}
	}
}

int main ( int argc, char* argv [] )
{
	char uinputbuf [ UINPUTBUFSZ ];

	// Switch to graphics mode
	GFX_init();

	//
	drawPalette();


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


	// Switch to text mode
	GFX_exit();

	exit();
}
