#include "types.h"
#include "memlayout.h"


// Record the current call stack in pcs[] by following the %ebp chain.

/* Stack during subroutine call:
   http://www.cs.virginia.edu/~evans/cs216/guides/x86.html

      -----------------
      local variable 2
      -----------------
      local variable 1
      -----------------
      local variable 0
      -----------------
      saved EBP
      -----------------
      return address
      -----------------
      parameter 0
      -----------------
      parameter 1
      -----------------
      parameter 2
      -----------------

   https://practicalmalwareanalysis.com/2012/04/03/all-about-ebp/
     . "At EBP is a pointer to EBP for the previous frame"
*/
void getcallerpcs ( void* param0, uint pcs [], uint npcs )
{
	uint* ebp;
	int   i;

	ebp = ( ( uint* ) param0 ) - 2;  // point to saved %ebp

	for ( i = 0; i < npcs; i += 1 )
	{
		if ( ebp == 0                      ||  // ??
			 ebp == ( uint* ) 0xffffffff   ||  // fake return ??
			 ebp <  ( uint* ) KERNBASE   )     // user space ??
		{
			break;
		}

		pcs[ i ] = *( ebp + 1 );   // saved return address (%eip)

		ebp = ( uint* ) ( *ebp );  // point to caller's saved %ebp
	}

	// Zero fill the rest
	while ( i < npcs )
	{
		pcs[ i ] = 0;

		i += 1;
	}
}
