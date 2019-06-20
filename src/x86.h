// Routines to let C code use special x86 instructions.

static inline uchar inb ( ushort port )
{
	uchar data;

	asm volatile( "in %1, %0" : "=a" ( data ) : "d" ( port ) );

	return data;
}

static inline void insl ( int port, void* addr, int cnt )
{
	asm volatile(

		"cld; rep insl" :
		"=D" ( addr ), "=c" ( cnt ) :
		"d" ( port ), "0" ( addr ), "1" ( cnt ) :
		"memory", "cc"
	);
}

static inline void outb ( ushort port, uchar data )
{
	asm volatile( "out %0, %1" : : "a" ( data ), "d" ( port ) );
}

static inline void outw ( ushort port, ushort data )
{
	asm volatile( "out %0, %1" : : "a" ( data ), "d" ( port ) );
}

static inline void outsl ( int port, const void* addr, int cnt )
{
	asm volatile(

		"cld; rep outsl" :
		"=S" ( addr ), "=c" ( cnt ) :
		"d" ( port ), "0" ( addr ), "1" ( cnt ) :
		"cc"
	);
}

static inline void stosb ( void* addr, int data, int cnt )
{
	asm volatile(

		"cld; rep stosb" :
		"=D" ( addr ), "=c" ( cnt ) :
		"0" ( addr ), "1" ( cnt ), "a" ( data ) :
		"memory", "cc"
	);
}

static inline void stosl ( void* addr, int data, int cnt )
{
	asm volatile(

		"cld; rep stosl" :
		"=D" ( addr ), "=c" ( cnt ) :
		"0" ( addr ), "1" ( cnt ), "a" ( data ) :
		"memory", "cc"
	);
}

struct segdesc;

// Load global descriptor table
static inline void lgdt ( struct segdesc* p, int size )
{
	volatile ushort pd[ 3 ];

	pd[ 0 ] = size - 1;
	pd[ 1 ] = ( uint )p;
	pd[ 2 ] = ( uint )p >> 16;

	asm volatile( "lgdt ( %0 )" : : "r" ( pd ) );
}

struct gatedesc;

// Load interrupt descriptor table
static inline void lidt ( struct gatedesc* p, int size )
{
	volatile ushort pd[ 3 ];

	pd[ 0 ] = size - 1;
	pd[ 1 ] = ( uint )p;
	pd[ 2 ] = ( uint )p >> 16;

	asm volatile( "lidt ( %0 )" : : "r" ( pd ) );
}

// Load task register
static inline void ltr ( ushort sel )
{
	asm volatile( "ltr %0" : : "r" ( sel ) );
}

static inline uint readeflags ( void )
{
	uint eflags;

	asm volatile( "pushfl; popl %0" : "=r" ( eflags ) );

	return eflags;
}

static inline void loadgs ( ushort v )
{
	asm volatile( "movw %0, %%gs" : : "r" ( v ) );
}

static inline void cli ( void )
{
	asm volatile( "cli" );
}

static inline void sti ( void )
{
	asm volatile( "sti" );
}

static inline uint xchg ( volatile uint* addr, uint newval )
{
	uint result;

	// The + in "+m" denotes a read-modify-write operand.
	asm volatile(

		"lock; xchgl %0, %1" :
		"+m" ( *addr ), "=a" ( result ) :
		"1" ( newval ) :
		"cc"
	);

	return result;
}

// Read CR2
static inline uint rcr2 ( void )
{
	uint val;

	asm volatile( "movl %%cr2, %0" : "=r" ( val ) );

	return val;
}

// Write CR3
static inline void lcr3 ( uint val )
{
	asm volatile( "movl %0, %%cr3" : : "r" ( val ) );
}

/* Layout of the trap frame built on the stack by the
   hardware and by trapasm.S, and passed to trap().

   The trapframe contains all the information necessary to
   restore the user mode CPU registers when the kernel returns,
   so that the user process can continue exactly as it was
   when the trap started.
*/
struct trapframe
{
	// Registers as pushed by pusha
	uint edi;
	uint esi;
	uint ebp;
	uint oesp;  // useless & ignored
	uint ebx;
	uint edx;
	uint ecx;
	uint eax;

	// Rest of trap frame
	ushort gs;
	ushort padding1;
	ushort fs;
	ushort padding2;
	ushort es;
	ushort padding3;
	ushort ds;
	ushort padding4;
	uint   trapno;    // pushed by vector#


	/** Below here defined by x86 hardware **/


	// If x86 does not push error code, pushed by vector#
	uint   err;

	// Saved by 'int' instruction
	uint   eip;       // ISR address
	ushort cs;
	ushort padding5;
	uint   eflags;

	// Saved by 'int' instruction when crossing rings, from user to kernel
	uint   esp;       // user stack pointer
	ushort ss;
	ushort padding6;
};

/* The "int n" instruction performs the following steps:

     . Fetches the n'th descritptor from the IDT

     . Checks that the current privilege level in %cs is equal
       to or higher than the privilege level (in the IDT entry)
       required for code to invoke the interrupt explicitly with
       the 'int' instruction
       . This allows the kernel to forbid int calls by a user program
         to ISRs it does not have permission for.
       . In such a case, a "general protection fault" is instead
         executed, "int 13"

     . Saves %esp and %ss in CPU-internal registers ??, but only
       if switching from user to kernel mode...

     . Loads %esp and %ss from a task segment descriptor ??, but only
       if switching from user to kernel mode...
       . The user stack cannot be used to save "trapframe" values...
       . Instead, the CPU uses the stack specified in the task segment...

     . pushes %ss      <-
     . pushes %esp       |
     . pushes %eflags    | become part of 'trapframe'
     . pushes %cs        |
     . pushes %eip     <-

       . note, only pushes %ss and %esp if crossing rings
       . In the case of an explicit 'int' instruction (ex. syscall),
         the pushed %eip would be the address of the instruction
         right after the 'int' instruction

     . Clears the IF flag (enable interrups) if an interrupt (vs. trap)...

     . Sets %cs and %eip to the values in the IDT entry
       . The instruction at the address in %eip is the next instruction to
         be executed and the first instruction of the interrupt handler...
*/
