#include "kernel/types.h"


// VGA modes
#define VGA_TXTMODE 0x03
#define VGA_GFXMODE 0x13

// Assuming VGA mode 0x13
#define SCREEN_WIDTH        320
#define SCREEN_HEIGHT       200
#define SCREEN_WIDTHxHEIGHT 64000  // 320 x 200

/* Corresponds to UMMIO value assigned in 'memlayout.h'
   Note, if 'memlayout.h' changes, make sure to update
   this address accordingly
*/
#define GFXBUFFER 0x7FFF0000  // VGA mode 0x13 framebuffer address


// ------------------------------------------------------------------------------

void GFX_init        ( void );
void GFX_exit        ( void );
void GFX_clearScreen ( uchar );
void GFX_drawPixel   ( int, int, uchar );
void GFX_blit        ( uchar* );
void GFX_fillRect    ( int, int, int, int, uchar );
