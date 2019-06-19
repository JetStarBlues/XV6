# Context switch
#
#   void swtch ( struct context** old, struct context* new );
# 
# Save the current registers on the stack, creating
# a struct context, and save its address in *old.
# Switch stacks to new and pop previously-saved registers.

.globl swtch

swtch:

	movl   4( %esp ), %eax  # eax = old
	movl   8( %esp ), %edx  # edx = new

	# Save old callee-saved registers
	# %eip is already on stack ??
	pushl  %ebp
	pushl  %ebx
	pushl  %esi
	pushl  %edi

	# Switch stacks
	movl   %esp, ( %eax )  # [old] = esp
	movl   %edx, %esp      # esp = new

	# Load new callee-saved registers
	popl   %edi
	popl   %esi
	popl   %ebx
	popl   %ebp

	ret
