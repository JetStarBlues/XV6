#include "syscall.h"
#include "traps.h"

# User code makes a system call with INT T_SYSCALL,
# System call number is in %eax, and arguments are on the stack.
# For example:
#    pushl $argv
#    pushl $path
#    pushl $0     # where caller pc would be...
#    movl  $SYS_exec, %eax
#    int   $T_SYSCALL

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
		int $T_SYSCALL;            \
		ret

SYSCALL( fork )
SYSCALL( exit )
SYSCALL( wait )
SYSCALL( pipe )
SYSCALL( read )
SYSCALL( write )
SYSCALL( close )
SYSCALL( kill )
SYSCALL( exec )
SYSCALL( open )
SYSCALL( mknod )
SYSCALL( unlink )
SYSCALL( fstat )
SYSCALL( link )
SYSCALL( mkdir )
SYSCALL( chdir )
SYSCALL( dup )
SYSCALL( getpid )
SYSCALL( sbrk )
SYSCALL( sleep )
SYSCALL( uptime )


# JK - above expands to (gcc -E):
# 
# .globl fork;   fork:   movl  $1, %eax; int $64; ret
# .globl exit;   exit:   movl  $2, %eax; int $64; ret
# .globl wait;   wait:   movl  $3, %eax; int $64; ret
# .globl pipe;   pipe:   movl  $4, %eax; int $64; ret
# .globl read;   read:   movl  $5, %eax; int $64; ret
# .globl write;  write:  movl $16, %eax; int $64; ret
# .globl close;  close:  movl $21, %eax; int $64; ret
# .globl kill;   kill:   movl  $6, %eax; int $64; ret
# .globl exec;   exec:   movl  $7, %eax; int $64; ret
# .globl open;   open:   movl $15, %eax; int $64; ret
# .globl mknod;  mknod:  movl $17, %eax; int $64; ret
# .globl unlink; unlink: movl $18, %eax; int $64; ret
# .globl fstat;  fstat:  movl  $8, %eax; int $64; ret
# .globl link;   link:   movl $19, %eax; int $64; ret
# .globl mkdir;  mkdir:  movl $20, %eax; int $64; ret
# .globl chdir;  chdir:  movl  $9, %eax; int $64; ret
# .globl dup;    dup:    movl $10, %eax; int $64; ret
# .globl getpid; getpid: movl $11, %eax; int $64; ret
# .globl sbrk;   sbrk:   movl $12, %eax; int $64; ret
# .globl sleep;  sleep:  movl $13, %eax; int $64; ret
# .globl uptime; uptime: movl $14, %eax; int $64; ret
