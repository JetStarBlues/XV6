# Initial process execs /init.
# This code runs in user space.

#include "syscall.h"
#include "traps.h"


# exec( char *path, char? *argv[] )
# exec( init, argv )
.globl start
start:

	pushl $argv
	pushl $init
	pushl $0     # where caller pc would be
	movl  $SYS_exec, %eax
	int   $T_SYSCALL

# for(;;) exit();
exit:

	movl $SYS_exit, %eax
	int  $T_SYSCALL
	jmp  exit

# char init[] = "/init\0";
init:

	.string "/init\0"

# char *argv[] = { init, 0 };
.p2align 2
argv:

	.long init  # argv[0] something about name, unix.stackexchange.com/questions/315812/
	.long 0

