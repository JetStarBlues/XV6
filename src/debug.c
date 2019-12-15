#include "types.h"
#include "memlayout.h"


// Record the current callstack into by following the %ebp chain.

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
     "EBP is a pointer to the top of the stack when the function
      is first called. By using a base pointer the return address
      will always be at ebp+4, the first parameter will always be
      at ebp+8, and the first local variable will always be at ebp-4.
      Even as the stack size grows and shrinks those offsets do not change...

      At ebp [ memory[ ebp ] ] is a pointer to ebp for the previous frame
      (this is why push ebp; mov ebp, esp is such a common way to start a
      function). This effectively creates a linked list of base pointers.
      This linked list makes it very easy to trace backwards up the stack.
     "
*/
void getcallerpcs ( void* firstParam, uint callstack [], uint depth )
{
	uint* saved_ebp_ptr;
	int   i;

	/* Use address of the first parameter to find the address
	   of the saved ebp in the associated stack frame.
	   I.e. get first ebp pointer in the call chain
	*/
	saved_ebp_ptr = ( ( uint* ) firstParam ) - 2;


	// Traverse "linked list" of %ebp to generate call stack
	for ( i = 0; i < depth; i += 1 )
	{
		/* Exit early if...
		*/
		if ( saved_ebp_ptr == 0                      ||  // ??
			 saved_ebp_ptr == ( uint* ) 0xffffffff   ||  // fake return ??
			 saved_ebp_ptr <  ( uint* ) KERNBASE   )     // user space ??
		{
			break;
		}


		/* Get the current stack frame's saved return address (%eip)
		   and place it into 'callstack'
		*/
		callstack[ i ] = *( saved_ebp_ptr + 1 );


		/* Evaluate the next stack frame in the call chain.
		   I.e. go to caller's stack frame...
		   I.e. get next ebp pointer in the call chain...
		*/
		saved_ebp_ptr = ( uint* ) ( *saved_ebp_ptr );
	}

	// Zero fill the rest
	while ( i < depth )
	{
		callstack[ i ] = 0;

		i += 1;
	}
}
