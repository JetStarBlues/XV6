#include "stdarg.h"  // GCC builtin

struct buf;
struct context;
struct file;
struct inode;
struct mouseStatus;
struct pipe;
struct proc;
struct rtcdate;
struct sleeplock;
struct spinlock;
struct stat;
struct superblock;

// bio.c
void            binit  ( void );
struct buf*     bread  ( uint, uint );
void            brelse ( struct buf* );
void            bwrite ( struct buf* );

// console.c
void            consoleinit ( void );
void            consoleintr ( int ( * ) ( void ) );
void            panic       ( char* ) __attribute__( ( noreturn ) );
void            cprintf     ( char*, ... );

// debug.c
void            getcallerpcs ( void*, uint*, uint );

// display.c
void            displayinit ( void );

// exec.c
int             exec ( char*, char* [] );

// file.c
struct file*    filealloc ( void );
void            fileclose ( struct file* );
struct file*    filedup   ( struct file* );
void            fileinit  ( void );
int             fileread  ( struct file*, char*, int n );
int             filestat  ( struct file*, struct stat* );
int             filewrite ( struct file*, char*, int n );

// fs.c
void            readsb      ( int dev, struct superblock *sb );
int             dirlink     ( struct inode*, char*, uint );
struct inode*   dirlookup   ( struct inode*, char*, uint* );
struct inode*   ialloc      ( uint, short );
struct inode*   idup        ( struct inode* );
void            iinit       ( int dev );
void            ilock       ( struct inode* );
void            iput        ( struct inode* );
void            iunlock     ( struct inode* );
void            iunlockput  ( struct inode* );
void            iupdate     ( struct inode* );
void            itrunc      ( struct inode* );  // JK - make public so can use for O_TRUNC...
int             namecmp     ( const char*, const char* );
struct inode*   namei       ( char* );
struct inode*   nameiparent ( char*, char* );
int             readi       ( struct inode*, char*, uint, uint );
void            stati       ( struct inode*, struct stat* );
int             writei      ( struct inode*, char*, uint, uint );

// ide.c
void            ideinit ( void );
void            ideintr ( void );
void            iderw   ( struct buf* );

// ioapic.c
extern uchar    ioapicid;
void            ioapicenable ( int irq, int cpu );
void            ioapicinit   ( void );

// kalloc.c
char*           kalloc ( void );
void            kfree  ( char* );
void            kinit1 ( void*, void* );
void            kinit2 ( void*, void* );

// kbd.c
void            kbdintr ( void );

// kprintf.c
int vkprintf ( void ( * ) ( int ), const char*, va_list );

// lapic.c
extern volatile uint *lapic;
void            cmostime     ( struct rtcdate *r );
int             lapicid      ( void );
void            lapiceoi     ( void );
void            lapicinit    ( void );
void            lapicstartap ( uchar, uint );
void            microdelay   ( int );

// log.c
void            begin_op  ( void );
void            end_op    ( void );
void            initlog   ( int dev );
void            log_write ( struct buf* );

// mouse.c
void            mouseinit      ( void );
void            mouseintr      ( void );
void            getMouseStatus ( struct mouseStatus* );

// mp.c
extern int      ismp;
void            mpinit ( void );

// picirq.c
void            picenable ( int );
void            picinit   ( void );

// pipe.c
int             pipealloc ( struct file**, struct file** );
void            pipeclose ( struct pipe*, int );
int             piperead  ( struct pipe*, char*, int );
int             pipewrite ( struct pipe*, char*, int );

// proc.c
int             cpuid     ( void );
void            exit      ( void );
int             fork      ( void );
int             growproc  ( int );
int             kill      ( int );
struct cpu*     mycpu     ( void );
struct proc*    myproc    ( void );
void            procdump  ( void );
void            procinit  ( void );
void            scheduler ( void ) __attribute__( ( noreturn ) );
void            sched     ( void );
void            setproc   ( struct proc* );
void            sleep     ( void*, struct spinlock* );
void            userinit  ( void );
int             wait      ( void );
void            wakeup    ( void* );
void            yield     ( void );

// swtch.S
void            swtch ( struct context**, struct context* );

// spinlock.c
void            acquire  ( struct spinlock* );
int             holding  ( struct spinlock* );
void            initlock ( struct spinlock*, char* );
void            popcli   ( void );
void            pushcli  ( void );
void            release  ( struct spinlock* );

// sleeplock.c
void            acquiresleep  ( struct sleeplock* );
int             holdingsleep  ( struct sleeplock* );
void            initsleeplock ( struct sleeplock*, char* );
void            releasesleep  ( struct sleeplock* );

// string.c
int             memcmp     ( const void*, const void*, uint );
void*           memcpy     ( void*, const void*, uint );
void*           memmove    ( void*, const void*, uint );
void*           memset     ( void*, int, uint );
char*           safestrcpy ( char*, const char*, int );
int             strlen     ( const char* );
int             strncmp    ( const char*, const char*, int );
char*           strncpy    ( char*, const char*, int );

// syscall.c
int             argint   ( int, int* );
int             argptr   ( int, char**, int );
int             argstr   ( int, char** );
int             fetchint ( uint, int* );
int             fetchstr ( uint, char** );
void            syscall  ( void );

// timer.c
void            timerinit ( void );

// trap.c
extern struct spinlock tickslock;
extern uint     ticks;
void            idtinit  ( void );
void            trapinit ( void );

// uart.c
void            uartinit ( void );
void            uartintr ( void );
void            uartputc ( int );

// vga.c
void            vgainit              ( void );
void            vgaputc              ( int );
void            vgaSetMode           ( int );
void            vgaSetDefaultPalette ( void );
void            vgaSetPaletteColor   ( int, char, char, char );

void            vgaHandleMouseEvent  ( void );
// void            demoGraphics         ( void );

// vm.c
int             allocuvm   ( pde_t*, uint, uint );
void            clearpteu  ( pde_t* pgdir, char* uva );
int             copyout    ( pde_t*, uint, void*, uint );
pde_t*          copyuvm    ( pde_t*, uint );
int             deallocuvm ( pde_t*, uint, uint );
void            freevm     ( pde_t* );
void            inituvm    ( pde_t*, char*, uint );
void            kvmalloc   ( void );
int             loaduvm    ( pde_t*, char*, struct inode*, uint, uint );
void            seginit    ( void );
pde_t*          setupkvm   ( void );
void            switchkvm  ( void );
void            switchuvm  ( struct proc* );

// number of elements in fixed-size array
#define NELEM( x ) ( sizeof( x ) / sizeof( ( x )[ 0 ] ) )

// minimum of two numbers
#define MIN( a, b ) ( ( a ) < ( b ) ? ( a ) : ( b ) )

// maximum of two numbers
// #define MAX( a, b ) ( ( a ) > ( b ) ? ( a ) : ( b ) )
