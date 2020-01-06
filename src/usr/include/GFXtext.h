#include "types.h"

void GFXText_setCursorPosition ( uint, uint );
void GFXText_getCursorPosition ( uint*, uint* );
void GFXText_getDimensions     ( uint*, uint* );
void GFXText_setTextColor      ( uchar );
void GFXText_setTextBgColor    ( uchar );
void GFXText_setCursorColor    ( uchar );
void GFXText_invertTextColors  ( void );
void GFXText_printChar         ( uchar );
void GFXText_drawCursor        ( void );
void GFXText_clearScreen       ( void );
