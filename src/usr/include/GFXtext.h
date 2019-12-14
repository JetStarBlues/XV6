#include "types.h"

void initGFXText       ( void );
void exitGFXText       ( void );
void setDisplayFd      ( int );
void setCursorPosition ( uint, uint );
void getCursorPosition ( uint*, uint* );
void setTextColor      ( uchar );
void setTextBgColor    ( uchar );
void setCursorColor    ( uchar );
void invertTextColors  ( void );
void clearScreen       ( void );
void printChar         ( uchar );
void drawCursor        ( void );
