// https://youtu.be/TPR8QAL_T4o

#include "kernel/types.h"
#include "user.h"

int main ( int argc, char* argv [] )
{
	int* x;

	x = 0;  // should raise fault when pointing to 0 ??

	/* vAddr 0 holds start of instructions.
	   With regards to security, instr_start..instr_end
	   shouldn't be accessible ??
	*/

	*x = 0xFF;  // put checks to prevent user from overwritting instructions ??

	/* Should user be able to modify program's instructions?
	   What is standard behaviour?
	*/

	printf( 1, "%d (0x%x)\n", *x, *x );

	exit();
}
