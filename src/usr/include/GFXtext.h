#include "types.h"

void GFXText_setCursorPosition ( int, int );
void GFXText_getCursorPosition ( int*, int* );
void GFXText_getDimensions     ( int*, int* );
void GFXText_setTextColor      ( uchar );
void GFXText_setTextBgColor    ( uchar );
void GFXText_setCursorColor    ( uchar );
void GFXText_invertTextColors  ( void );
void GFXText_printChar         ( uchar );
void GFXText_drawCursor        ( void );
void GFXText_clearScreen       ( void );
