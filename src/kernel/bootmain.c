// Boot loader.
//
// Part of the boot block, along with bootasm.S, which calls bootmain().
// bootasm.S has put the processor into protected 32-bit mode.
// bootmain() loads an ELF kernel image from the disk starting at
// sector 1 and then jumps to the kernel entry routine.

/* When a x86 PC boots, it starts by executing a program called the BIOS.
   The BIOS's job is to prepare the hardware and then transfer control
   to the operating system.
   Specifically, it transfers control to code loaded from the boot sector,
   the first 512-byte sector of the boot disk.
   The boot sector contains the bootloader - instructions that load the
   kernel into memory.

   The BIOS loads the boot sector at memory address 0x7C00 and then
   jumps to that address (sets %eip = 0x7C00).
   As such, the make rule in makefile to create the bootblock uses the
   "-Ttext 0x7C00" flag

   When the boot loader begins executing, the x86 CPU is simulating an
   Intel 8088.
   The boot loader's job is:
     . to put it into x86 mode, (bootasm.S)
     . load the xv6 kernel from disk to memory, (bootmain.c)
     . then transfer control to the kernel (bootmain.c)

   We expect to find a copy of the kernel executable (ELF) on the disk
   starting at the second sector ??

      ----------
      bootblock   sector 0 (512 bytes)
      ----------
      kernel      sector 1..x
      ----------

   We assume that the bootblock and kernel have been written to the
   boot disk contiguously.
*/

#include "types.h"
#include "elf.h"
#include "x86.h"
#include "memlayout.h"
#include "ide.h"

void readsect ( void*, uint );
void readseg  ( uchar*, uint, uint );

void bootmain ( void )
{
	struct elfhdr*  elf;
	struct proghdr* ph;
	struct proghdr* eph;
	uchar*          pa;

	/* Function pointer declaration
	   entry is a function that takes no arguments and returns no values
	   https://www.cprogramming.com/tutorial/function-pointers.html
	*/
	void ( *entry ) ( void );


	elf = ( struct elfhdr* ) 0x10000;  // allocate in scratch space...
	                                   /* RAM between 0x0000_7E00 and 0x0007_FFFF is guaranteed available for use.
	                                       https://wiki.osdev.org/Memory_Map_(x86) */

	// Read 1st page off disk (first sector of kernel?)
	readseg( ( uchar* ) elf, 4096, 0 );

	// Is this an ELF executable?
	if ( elf->magic != ELF_MAGIC )
	{
		return;  // let bootasm.S handle error
	}

	// Load each program segment (ignores ph flags)
	ph = ( struct proghdr* ) ( ( uchar* ) elf + elf->phoff );

	eph = ph + elf->phnum;

	for ( ; ph < eph; ph += 1 )
	{
		pa = ( uchar* ) ph->paddr;

		readseg( pa, ph->filesz, ph->off );

		// Zero remainder of segment
		// ELF probably truncates trailing zeros for space efficiency
		if ( ph->memsz > ph->filesz )
		{
			stosb( pa + ph->filesz, 0, ph->memsz - ph->filesz );
		}
	}

	/* Call the entry point in the ELF header.
	   Does not return!

	   The entry point is the location the kernel expects to
	   start executing from.
	   By convention, the _start symbol specifies the ELF entry point...
	   It is defined in entry.S
	*/
	entry = ( void ( * ) ( void ) )( elf->entry );

	entry();
}


// _______________________________________________________________________

/* Read 'count' bytes at 'offset' from disk
   into memory addresss 'pAddr'.
   Might copy more than asked.
*/
void readseg ( uchar* pAddr, uint count, uint offset )
{
	uchar* endPAddr;

	endPAddr = pAddr + count;

	// Round down to sector boundary.
	pAddr -= offset % SECTOR_SIZE;

	// Translate from bytes to sectors; kernel starts at sector 1.
	offset = ( offset / SECTOR_SIZE ) + 1;

	// If this is too slow, we could read lots of sectors at a time.
	// We'd write more to memory than asked, but it doesn't matter --
	// we load in increasing order.
	for ( ; pAddr < endPAddr; pAddr += SECTOR_SIZE )
	{
		readsect( pAddr, offset );

		offset += 1;
	}
}


// _______________________________________________________________________

// Similar to idewait...
void waitdisk ( void )
{
	// Wait for disk ready
	while ( ( inb( REG_STATUS ) & ( IDE_STATUS_BSY | IDE_STATUS_DRDY ) ) != IDE_STATUS_DRDY )
	{
		//
	}
}

// Read a single sector at offset into dst.
// Similar to idestart and ideintr...
void readsect ( void* dst, uint offset )
{
	// Issue command
	waitdisk();

	outb( REG_SECTOR_COUNT, 1 );   // number of sectors
	outb( REG_SECTOR_NUMBER, offset );
	outb( REG_LOW_ADDRESS,   offset >> 8 );
	outb( REG_HIGH_ADDRESS,  offset >> 16 );
	outb( REG_DRIVE_SELECT,  ( offset >> 24 ) | USE_LBA_ADDR );

	outb( REG_STATUS, IDE_CMD_READ );


	// Read data
	waitdisk();

	insl( REG_DATA, dst, SECTOR_SIZE / 4 );
}
