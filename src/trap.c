#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "traps.h"
#include "spinlock.h"

// Interrupt descriptor table ( shared by all CPUs ).
struct gatedesc idt     [ 256 ];
extern uint     vectors [];  // in vectors.S: array of 256 entry pointers
struct spinlock tickslock;
uint            ticks;

void tvinit ( void )
{
	int i;

	for ( i = 0; i < 256; i += 1 )
	{
		SETGATE( idt[ i ], 0, SEG_KCODE << 3, vectors[ i ], DPL_KERN );
	}

	// If system call, do not disable interrupts.
	// Also set privilege level to DPL_USER.
	SETGATE( idt[ T_SYSCALL ], 1, SEG_KCODE << 3, vectors[ T_SYSCALL ], DPL_USER );

	initlock( &tickslock, "time" );
}

void idtinit ( void )
{
	lidt( idt, sizeof( idt ) );
}

//PAGEBREAK: 41
void trap ( struct trapframe *tf )
{
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

		case T_IRQ0 + IRQ_IDE:

			ideintr();

			lapiceoi();

			break;

		case T_IRQ0 + IRQ_IDE + 1:

			// Bochs generates spurious IDE1 interrupts.
			break;

		case T_IRQ0 + IRQ_KBD:

			kbdintr();

			lapiceoi();

			break;

		case T_IRQ0 + IRQ_COM1:

			uartintr();

			lapiceoi();

			break;

		case T_IRQ0 + 7:
		case T_IRQ0 + IRQ_SPURIOUS:

			/*cprintf(

				"cpu%d - spurious interrupt at\n"
				"    cs  : 0x%x\n"
				"    eip : 0x%x\n\n",

				cpuid(), tf->cs, tf->eip
			);*/

			cprintf( "cpu%d - spurious interrupt at\n", cpuid() );
			cprintf( "    cs  : 0x%x\n",                tf->cs  );
			cprintf( "    eip : 0x%x\n\n",              tf->eip );

			lapiceoi();

			break;

		//PAGEBREAK: 13
		default:

			// In kernel, it must be our mistake.
			if ( myproc() == 0 || ( tf->cs & 3 ) == DPL_KERN )
			{
				/*cprintf(

					"Unexpected kernel trap\n"
					"    trap : %d\n"
					"    cpu  : %d\n"
					"    eip  : 0x%x\n"
					"    cr2  : 0x%x\n\n",

					tf->trapno,
					cpuid(),
					tf->eip,
					rcr2()
				);*/

				cprintf( "Unexpected kernel trap\n" );
				cprintf( "    trap : %d\n",     tf->trapno );
				cprintf( "    cpu  : %d\n",     cpuid()    );
				cprintf( "    eip  : 0x%x\n",   tf->eip    );
				cprintf( "    cr2  : 0x%x\n\n", rcr2()     );

				panic( "trap" );
			}

			// In user space, assume process misbehaved.
			/*cprintf(

				"Kill misbehaved user process:\n"
				"    pid  : %d\n"
				"    name : %s\n"
				"    trap : %d\n"
				"    err  : %d\n"
				"    cpu  : %d\n"
				"    eip  : 0x%x\n"
				"    cr2  : 0x%x\n\n",

				myproc()->pid,
				myproc()->name,
				tf->trapno,
				tf->err,
				cpuid(),
				tf->eip,
				rcr2()
			);*/

			cprintf( "Kill misbehaved user process:\n" );
			cprintf( "    pid  : %d\n",     myproc()->pid  );
			cprintf( "    name : %s\n",     myproc()->name );
			cprintf( "    trap : %d\n",     tf->trapno     );
			cprintf( "    err  : %d\n",     tf->err        );
			cprintf( "    cpu  : %d\n",     cpuid()        );
			cprintf( "    eip  : 0x%x\n",   tf->eip        );
			cprintf( "    cr2  : 0x%x\n\n", rcr2()         );

			myproc()->killed = 1;
	}

	// Force process exit if it has been killed and is in user space.
	// (If it is still executing in the kernel, let it keep running
	// until it gets to the regular system call return.)
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
}
