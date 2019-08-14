#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

/* There are three cases when control must be transferred from a user
   program to the kernel:
     . When a user program asks the OS for a service via a syscall
     . When a user program performs an illegal action and thus raises
       an exception (ex. divide by zero, page fault)
     . When an external device generates a signal to indicate that it
       needs attention from the OS

    In a x86 CPU, these three events are traditionally handled by a
    single hardware mechanism - the interrupt.

    The OS must arrange for the following to happen:

      . The processor's registers must be saved for future transparent resume
        . xv6 does this in 'alltraps'...

      . Choose a place for the kernel to start executing
        . see IDT entries

      . The kernel must be able to retrieve information about the event
        ex. system call arguments
        . In xv6, system call number is saved in %eax. The number can
          later be used to determine how to read arguments from the
          user stack. See 'argint'
*/

struct gatedesc idt     [ 256 ];  // Interrupt descriptor table (shared by all CPUs).
extern uint     vectors [];       // in vectors.S: array of 256 entry pointers

struct spinlock tickslock;        // Must hold tickslock when modifying ticks
uint            ticks;            // Where is this initialized to 0?

// Initialize IDT
void tvinit ( void )
{
	int i;

	for ( i = 0; i < 256; i += 1 )
	{
		SETGATE( idt[ i ], 0, SEG_KCODE << 3, vectors[ i ], DPL_KERN );
	}

	/* If system call, do not disable interrupts.
	   Also set privilege level to DPL_USER (this allows a user program
	   to explicitly call "int $T_SYSCALL")
	*/
	SETGATE( idt[ T_SYSCALL ], 1, SEG_KCODE << 3, vectors[ T_SYSCALL ], DPL_USER );

	initlock( &tickslock, "time" );
}

void idtinit ( void )
{
	lidt( idt, sizeof( idt ) );
}

void trap ( struct trapframe *tf )
{
	// System call
	if ( tf->trapno == T_SYSCALL )
	{
		if ( myproc()->killed )
		{
			exit();
		}

		myproc()->tf = tf;

		syscall();

		if ( myproc()->killed )
		{
			exit();
		}

		return;
	}

	switch ( tf->trapno )
	{
		// Timer interrupt
		case T_IRQ0 + IRQ_TIMER:

			if ( cpuid() == 0 )
			{
				acquire( &tickslock );

				ticks += 1;

				wakeup( &ticks );

				release( &tickslock );
			}

			lapiceoi();

			break;


		// Disk interrupt
		case T_IRQ0 + IRQ_IDE:

			ideintr();

			lapiceoi();

			break;

		case T_IRQ0 + IRQ_IDE + 1:

			// Bochs generates spurious IDE1 interrupts.
			break;


		// Keyboard interrupt
		case T_IRQ0 + IRQ_KBD:

			kbdintr();

			lapiceoi();

			break;


		// Serial interrupt
		case T_IRQ0 + IRQ_COM1:

			uartintr();

			lapiceoi();

			break;


		// Mouse interrupt
		case T_IRQ0 + IRQ_MOUSE:

			mouseintr();

			lapiceoi();

			break;


		// Spurious
		case T_IRQ0 + 7:
		case T_IRQ0 + IRQ_SPURIOUS:

			cprintf(

				"cpu%d - spurious interrupt at\n"
				"    cs  : 0x%x\n"
				"    eip : 0x%x\n\n",

				cpuid(), tf->cs, tf->eip
			);

			lapiceoi();

			break;


		// Page fault
		/* Lazy page allocation
		     https://pdos.csail.mit.edu/6.828/2018/lec/l-usingvm.pdf
		     http://panda.moyix.net/~moyix/cs3224/fall16/hw5/hw5.html
		     http://pages.cs.wisc.edu/~cs537-3/Projects/p3b.html

		   For heap and stack...
		*/
		/*
		case T_PGFLT:

			uint vaddr = PGROUNDDOWN( rcr2() );
			
			cprintf( "page fault - attempted to access virtual address %x\n", vaddr );

			if ( vaddr < myproc()->sz )
			{
				//
				char* newpage = kalloc();

				if ( newpage == 0 )
				{
					cprintf( "lazy page allocation - out of memory\n" );

					exit();  // to where?
				}

				//
				memset( newpage, 0, PGSIZE );

				//
				if (
					mappages(

						myproc()->pgdir,
						( char* ) vaddr,  // virtual start address
						PGSIZE,           // size
						V2P( newpage ),   // physical start address
						PTE_W | PTE_U
					) < 0 )
				{
					kfree( newpage );

					cprintf( "lazy page allocation - mappages error\n" );

					exit();  // to where?
				}

				//
				cprintf( "page fault - successful lazy page allocation for virtual address %x\n", vaddr );

				return;
			}

			break;
		*/


		// Default
		default:

			// In kernel, it must be our mistake.
			if ( myproc() == 0 || ( tf->cs & 3 ) == DPL_KERN )
			{
				cprintf( "Unexpected kernel trap\n" );
				cprintf( "    trapno      : %d\n",     tf->trapno );
				cprintf( "    cpu         : %d\n",     cpuid()    );
				cprintf( "    eip         : 0x%x\n",   tf->eip    );
				cprintf( "    cr2 (vaddr) : 0x%x\n\n", rcr2()     );

				panic( "trap" );
			}

			// In user space, assume process misbehaved.
			else
			{
				cprintf( "Kill misbehaved user process:\n" );
				cprintf( "    pid         : %d\n",     myproc()->pid  );
				cprintf( "    name        : %s\n",     myproc()->name );
				cprintf( "    trapno      : %d\n",     tf->trapno     );
				cprintf( "    errno       : %d\n",     tf->err        );
				cprintf( "    cpu         : %d\n",     cpuid()        );
				cprintf( "    eip         : 0x%x\n",   tf->eip        );
				cprintf( "    cr2 (vaddr) : 0x%x\n\n", rcr2()         );

				// Kill the process
				myproc()->killed = 1;
			}
	}


	// Force process exit if it has been killed and is in user space.
	// If it is still executing in the kernel, let it keep running
	// until it gets to the regular system call return.
	if ( myproc() && myproc()->killed && ( tf->cs & 3 ) == DPL_USER )
	{
		exit();
	}

	// Force process to give up CPU on clock tick.
	// If interrupts were on while locks held, would need to check nlock.
	if ( myproc() && myproc()->state == RUNNING &&
		 tf->trapno == T_IRQ0 + IRQ_TIMER )
	{
		yield();
	}

	// Check if the process has been killed since we yielded
	if ( myproc() && myproc()->killed && ( tf->cs & 3 ) == DPL_USER )
	{
		exit();
	}


	// Returns to trapret
}
