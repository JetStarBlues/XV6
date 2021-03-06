# Initial process execs /init.
# This code runs in user space.

#include "syscall.h"
#include "trap.h"


# exec( char* path, char* argv [] )
.globl start
start:

	pushl $argv
	pushl $init
	pushl $0     # Fake the return address that would have been pushed by a 'call' instruction...
	             # 'argint' which is used to fetch argumets from the stack, exepects
	             # a return position to be present in stack

	movl  $SYS_exec, %eax
	int   $T_SYSCALL


# If all goes well, exec should not return
# If exec fails and returns, we loop calling the
# exit syscall which definitely should not return...

# for(;;) exit();
exit:

	movl $SYS_exit, %eax
	int  $T_SYSCALL
	jmp  exit


# char init [] = "/init\0";
init:

	# .string "/init\0"
	.string "/bin/init\0"

# char* argv [] = { init, 0 };
.p2align 2
argv:

	.long init  # argv[0] something about name, unix.stackexchange.com/questions/315812/
	.long 0     # mark end of argv array

