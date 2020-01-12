/* Simple linker script for the JOS kernel.
   See the GNU ld 'info' manual ("info ld") to learn the syntax. */

OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)

SECTIONS
{
	/* Link the kernel at run-time address 0x80100000.
	 * This address must be equal to KERNLINK.
	 *
	 * The "." is a location counter, that holds the value of the
	 * current address.
	 */
	. = 0x80100000;


	/* The 'text' section is placed at run-time address 0x80100000,
	 * and load address 0x100000.
	 */
	.text : AT(0x100000) {

		*(.text .stub .text.* .gnu.linkonce.t.*)
	}


	/* Define the 'etext' symbol to this value
	 */
	PROVIDE(etext = .);


	/* The 'rodata' section follows the 'text' section
	 */
	.rodata : {

		*(.rodata .rodata.* .gnu.linkonce.r.*)
	}


	/* Include debugging information in kernel memory.
	 * The 'stab' and 'stabstr' sections follow the 'rodata' section
	 */
	.stab : {

		PROVIDE(__STAB_BEGIN__ = .);
		*(.stab);
		PROVIDE(__STAB_END__ = .);

		/* Force the linker to allocate space for this section
		 *
		 * JK:
		 *  commented this out. For some reason,
		 *   . triggers an "ld: warning: dot moved backwards before `.stab'" error
		 *     when using gcc-multilib, and
		 *   . the generated kernel fails to boot.
		 *
		 *  TODO: confirm omission has no side effects
		*/
		/* BYTE(0) */
	}

	.stabstr : {

		PROVIDE(__STABSTR_BEGIN__ = .);
		*(.stabstr);
		PROVIDE(__STABSTR_END__ = .);

		/* Force the linker to allocate space for this section
		 *
		 * JK:
		 *  commented this out. For some reason,
		 *   . triggers an "ld: warning: dot moved backwards before `.stab'" error
		 *     when using gcc-multilib, and
		 *   . the generated kernel fails to boot.
		 *
		 *  TODO: confirm omission has no side effects
		*/
		/* BYTE(0) */
	}


	/* Adjust the address for the data segment to the next page
	 */
	. = ALIGN(0x1000);


	/* Conventionally, Unix linkers provide pseudo-symbols
	 * 'etext', 'edata', and 'end', at the end of the 'text', 'data', and 'bss'.
	 *
	 * For the kernel mapping, we need the address at the beginning
	 * of the data section, but that's not one of the conventional
	 * symbols, because the convention started before there was a
	 * read-only rodata section between text and data.
	 *
	 * Thus ??
	*/

	/* The 'data' segment
	 */
	PROVIDE(data = .);

	.data : {

		*(.data)
	}

	PROVIDE(edata = .);


	/* The 'bss' segment
	 */
	.bss : {

		*(.bss)
	}


	/* Mark 'end' of kernel
	 */
	PROVIDE(end = .);


	/* Discard all other sections in the constituent object files...
	 */
	/DISCARD/ : {

		*(.eh_frame .note.GNU-stack)
	}
}
