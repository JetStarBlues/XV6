# Context switch
#
#   void swtch ( struct context** oldContext, struct context* newContext );
# 
# Save the current context as "oldContext" and load the new one "newContext"
# 
# Save the current registers on the current stack, creating a "struct context",
# and save its address in *oldContext.
# Switch to newContext's stack and pop its previously-saved registers.
#
# %esp is saved implicitly as the address of the "struct context"

.globl swtch

swtch:

	movl   4( %esp ), %eax  # eax = oldContext
	movl   8( %esp ), %edx  # edx = newContext

	# Save oldContext's callee-saved registers
	# %eip is already on stack... (placed by C compiler in
	#  accordance with calling convention as return address "0( % esp )")
	# ...  %eip   #  <-
	pushl  %ebp   #    |
	pushl  %ebx   #    | oldContext
	pushl  %esi   #    |
	pushl  %edi   #  <-

	# Switch stacks
	movl   %esp, ( %eax )  # [oldContext] = esp
	movl   %edx, %esp      # esp = newContext

	# Load newContext's callee-saved registers
	popl   %edi  #  <-
	popl   %esi  #    |
	popl   %ebx  #    | newContext
	popl   %ebp  #    |
	# ..   %eip  #  <-

	ret
