#include "kernel/types.h"

void GFXText_getDimensions     ( int*, int* );

void GFXText_setCursorPosition ( int, int );
void GFXText_getCursorPosition ( int*, int* );

void GFXText_setTextColor      ( uchar );
void GFXText_setTextBgColor    ( uchar );
void GFXText_setCursorColor    ( uchar );
uchar GFXText_getTextColor     ( void );
uchar GFXText_getTextBgColor   ( void );
uchar GFXText_getCursorColor   ( void );

void GFXText_invertTextColors  ( void );

void GFXText_useBoldface       ( int );

void GFXText_printChar         ( uchar );
void GFXText_drawCursor        ( void );
void GFXText_clearScreen       ( void );
