# The xv6 kernel starts executing in this file. This file is linked with
# the kernel C code, so it can refer to kernel symbols such as main().
# The boot block (bootasm.S and bootmain.c) jumps to entry below.
				
# Multiboot header, for multiboot boot loaders like GNU Grub.
# http://www.gnu.org/software/grub/manual/multiboot/multiboot.html
#
# Using GRUB 2, you can boot xv6 from a file stored in a
# Linux file system by copying kernel or kernelmemfs to /boot
# and then adding this menu entry:
#
# menuentry "xv6" {
# 	insmod ext2
# 	set root='(hd0,msdos1)'
# 	set kernel='/boot/kernel'
# 	echo "Loading ${kernel}..."
# 	multiboot ${kernel} ${kernel}
# 	boot
# }

# JK
# WIP attempt to have better video mode:
# Based on:
#   . https://github.com/SerenityOS/serenity/blob/master/Kernel/Arch/i386/Boot/boot.S
#   . https://github.com/SerenityOS/serenity/blob/master/Kernel/Multiboot.h
#   . https://github.com/SerenityOS/serenity/blob/master/Kernel/init.cpp
#   . https://www.gnu.org/software/grub/manual/multiboot/html_node/boot_002eS.html#boot_002eS
#   . https://www.gnu.org/software/grub/manual/multiboot/html_node/multiboot_002eh.html

#include "segasm.h"
#include "memlayout.h"
#include "mmu.h"
#include "param.h"


# Multiboot header. Data to direct multiboot loader.
.p2align 2
.text
.globl multiboot_header

#if 1
	multiboot_header:

		#define MULTIBOOT_MAGIC     0x1badb002
		#define MULTIBOOT_FLAGS     0
		#define MULTIBOOT_CHECKSUM  - ( MULTIBOOT_MAGIC + MULTIBOOT_FLAGS )

		.long MULTIBOOT_MAGIC
		.long MULTIBOOT_FLAGS
		.long MULTIBOOT_CHECKSUM
#endif

#if 0
	multiboot_header:

		#define MULTIBOOT_MAGIC       0x1badb002
		#define MULTIBOOT_VIDEO_MODE  0x4
		#define MULTIBOOT_FLAGS       MULTIBOOT_VIDEO_MODE
		#define MULTIBOOT_CHECKSUM    - ( MULTIBOOT_MAGIC + MULTIBOOT_FLAGS )

		.long MULTIBOOT_MAGIC
		.long MULTIBOOT_FLAGS
		.long MULTIBOOT_CHECKSUM

		# For MULTIBOOT_MEMORY_INFO
		.long 0x00000000    # header_addr
		.long 0x00000000    # load_addr
		.long 0x00000000    # load_end_addr
		.long 0x00000000    # bss_end_addr
		.long 0x00000000    # entry_addr

		# For MULTIBOOT_VIDEO_MODE
		.long 0x00000000    # mode_type
		.long 1280          # width
		.long 1024          # height
		.long 32            # depth
#endif


# By convention, the _start symbol specifies the ELF entry point.
# Since we haven't set up virtual memory yet, our entry point is
# the physical address of 'entry'.
.globl _start
_start = V2P_WO( entry )


# Entering xv6 on boot processor, with paging off.
.globl entry
entry:

	# Turn on page size extension for 4Mbyte "super" pages
	movl  %cr4, %eax
	orl   $( CR4_PSE ), %eax
	movl  %eax, %cr4

	# Set page directory
	movl  $( V2P_WO( entrypgdir ) ), %eax
	movl  %eax, %cr3

	# Turn on paging
	movl  %cr0, %eax
	orl   $( CR0_PG | CR0_WP ), %eax
	movl  %eax, %cr0

	# Set up the stack pointer
	movl  $( stack + KSTACKSIZE ), %esp

	# Jump to main(), and switch to executing at
	# high addresses. The indirect call is needed because
	# the assembler produces a PC-relative instruction
	# for a direct jump.
	mov  $main, %eax
	jmp  *%eax


.comm stack, KSTACKSIZE  # allocate space (bss)...
