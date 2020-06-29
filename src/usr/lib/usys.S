#include "kernel/syscall.h"
#include "kernel/traps.h"

# User code makes a system call with "int $T_SYSCALL"
# System call number is in %eax, and arguments are on the stack.

# For example, the line "exec( my_path, my_argv )", is compiled as:
#
#    pushl $my_argv
#    pushl $my_path
#    call  exec
#
# If we decompose the 'call' instruction, into its psuedo components
#  (stackoverflow.com/a/523550), we get something like:
#
#    pushl $my_argv
#    pushl $my_path
#    pushl returnAddress
#    jmp   exec
#
# The "call exec" goes to the following code we generate below:
#
#    exec:
#        movl $SYS_exec, %eax
#        int  $T_SYSCALL
#        ret


# Concatenate tokens using the ## operator - C preprocessor
# https://wiki.sei.cmu.edu/confluence/display/c/PRE01-C.+Use+parentheses+within+macros+around+parameter+names
#    #define JOIN(a, b) (a ## b)

# Semicolon to delimit lines in single line assembly
# https://stackoverflow.com/questions/27685548


#define SYSCALL( name )            \
                                   \
	.globl name;                   \
	name:                          \
                                   \
		movl $SYS_ ## name, %eax;  \
		int  $T_SYSCALL;           \
		ret

SYSCALL( chdir   )
SYSCALL( close   )
SYSCALL( dup     )
SYSCALL( exec    )
SYSCALL( exit    )
SYSCALL( fork    )
SYSCALL( fstat   )
SYSCALL( getpid  )
SYSCALL( gettime )
SYSCALL( ioctl   )
SYSCALL( kill    )
SYSCALL( link    )
SYSCALL( lseek   )
SYSCALL( mkdir   )
SYSCALL( mknod   )
SYSCALL( open    )
SYSCALL( pipe    )
SYSCALL( read    )
SYSCALL( sbrk    )
SYSCALL( sleep   )
SYSCALL( unlink  )
SYSCALL( uptime  )
SYSCALL( wait    )
SYSCALL( write   )


# JK - above expands to (gcc -E):
# 
# .globl chdir;   chdir:   movl $1,  %eax; int $64; ret
# .globl close;   close:   movl $2,  %eax; int $64; ret
# .globl dup;     dup:     movl $3,  %eax; int $64; ret
# .globl exec;    exec:    movl $4,  %eax; int $64; ret
# .globl exit;    exit:    movl $5,  %eax; int $64; ret
# .globl fork;    fork:    movl $6,  %eax; int $64; ret
# .globl fstat;   fstat:   movl $7,  %eax; int $64; ret
# .globl getpid;  getpid:  movl $8,  %eax; int $64; ret
# .globl gettime; gettime: movl $9,  %eax; int $64; ret
# .globl ioctl;   ioctl:   movl $10, %eax; int $64; ret
# .globl kill;    kill:    movl $11, %eax; int $64; ret
# .globl link;    link:    movl $12, %eax; int $64; ret
# .globl lseek;   lseek:   movl $13, %eax; int $64; ret
# .globl mkdir;   mkdir:   movl $14, %eax; int $64; ret
# .globl mknod;   mknod:   movl $15, %eax; int $64; ret
# .globl open;    open:    movl $16, %eax; int $64; ret
# .globl pipe;    pipe:    movl $17, %eax; int $64; ret
# .globl read;    read:    movl $18, %eax; int $64; ret
# .globl sbrk;    sbrk:    movl $19, %eax; int $64; ret
# .globl sleep;   sleep:   movl $20, %eax; int $64; ret
# .globl unlink;  unlink:  movl $21, %eax; int $64; ret
# .globl uptime;  uptime:  movl $22, %eax; int $64; ret
# .globl wait;    wait:    movl $23, %eax; int $64; ret
# .globl write;   write:   movl $24, %eax; int $64; ret
