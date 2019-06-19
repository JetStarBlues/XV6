#include "mmu.h"

# vectors.S sends all traps here ('alltraps').
# I.e. this is the interrupt handler for all interrupts
#
# 'alltraps' then forwards the interrupt handling to the
# 'trap' C function
#
# Before doing so, 'alltraps' first prepares the "struct trapframe"
# that the 'trap' function expects as an argument.
.globl alltraps
alltraps:

	# Build trap frame.
	pushl   %ds
	pushl   %es
	pushl   %fs
	pushl   %gs
	pushal
	
	# Set up data segments.
	movw    $( SEG_KDATA << 3 ), %ax
	movw    %ax, %ds
	movw    %ax, %es

	# Call trap( tf ), where tf = %esp
	pushl   %esp
	call    trap      # in trap.c
	addl    $4, %esp


# Return (from trap.c?) falls through to trapret...
# 'allocproc' also configures new processes to end up in trapret...
#
# The 'reti' x86 instruction causes a switch from kernel to user mode...
#  https://stackoverflow.com/a/13262538
.globl trapret
trapret:

	popal
	popl   %gs
	popl   %fs
	popl   %es
	popl   %ds
	addl   $0x8, %esp  # skip over trapno and errcode ??
	iret               # pops %eip, %cs, %eflags, %esp, %ss...
