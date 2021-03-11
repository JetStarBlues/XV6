# Useful Makefile reference:
#   . https://www.gnu.org/software/make/manual/make.html#Automatic-Variables


# ___ Paths _________________________________________________________________

# --- Directories -----------------------------------------------------------

SRCDIR          = src/
KERNDIR         = src/kernel/
KERNHEADERDIR   = src/kernel/
USERHEADERDIR   = src/usr/include/
UPROGDIR        = src/usr/prog/
UPROGCOREDIR    = src/usr/prog/core/
UPROGAUXDIR     = src/usr/prog/auxiliary/
UPROGAUXTESTDIR = src/usr/prog/auxiliary/test/
UPROGAUXWISCDIR = src/usr/prog/auxiliary/wisc/
ULIBDIR         = src/usr/lib/
KERNBINDIR      = bin/kern/
UTILBINDIR      = bin/util/
USERBINDIR      = bin/user/
DEBUGDIR        = debug/
DOCDIR          = doc/
IMGDIR          = img/

FSDIR           = fs/
FSBINDIR        = fs/bin/
FSDEVDIR        = fs/dev/
FSUSRDIR        = fs/usr/
FSUSRBINDIR     = fs/usr/bin/
FSUSRBINTESTDIR = fs/usr/bin/test/
FSUSRBINWISCDIR = fs/usr/bin/wisc/


# --- Kernel code -----------------------------------------------------------

_KERN_HEADERS =  \
	buf.h        \
	date.h       \
	defs.h       \
	display.h    \
	elf.h        \
	fcntl.h      \
	file.h       \
	fs.h         \
	kbd.h        \
	kfonts.h     \
	memlayout.h  \
	mmu.h        \
	mp.h         \
	param.h      \
	proc.h       \
	ps2.h        \
	segasm.h     \
	sleeplock.h  \
	spinlock.h   \
	stat.h       \
	syscall.h    \
	termios.h    \
	trap.h       \
	types.h      \
	vga.h        \
	x86.h

_KERN_OBJS =        \
	buf.o           \
	console.o       \
	debug.o         \
	display.o       \
	exec.o          \
	file.o          \
	fs.o            \
	ide.o           \
	ioapic.o        \
	kalloc.o        \
	kbd.o           \
	kprintf.o       \
	lapic.o         \
	log.o           \
	main.o          \
	mouse.o         \
	mp.o            \
	picirq.o        \
	pipe.o          \
	proc.o          \
	sleeplock.o     \
	spinlock.o      \
	string.o        \
	swtch.o         \
	syscall.o       \
	sysfile.o       \
	sysmisc.o       \
	sysproc.o       \
	trap.o          \
	trapasm.o       \
	uart.o          \
	trapvectors.o   \
	vga.o           \
	vm.o


# --- User code -------------------------------------------------------------

_USER_HEADERS = \
	fonts.h     \
	GFX.h       \
	GFXtext.h   \
	stdarg2.h   \
	time.h      \
	user.h

_ULIB_OBJS =   \
	GFX.o      \
	GFXtext.o  \
	printf.o   \
	termios.o  \
	time.o     \
	ulib.o     \
	umalloc.o  \
	usys.o

# TODO - Currently each folder is explicitly spelled out in Makefile
#        Would be great if somehow traversed directory and
#        generated paths automatically

_UPROG_OBJS =         \
	clock.o           \
	find.o            \
	hexdump.o         \
	keditor.o

_UPROG_CORE_OBJS =    \
	cat.o             \
	echo.o            \
	grep.o            \
	init.o            \
	kill.o            \
	ln.o              \
	ls.o              \
	mkdir.o           \
	rm.o              \
	sh.o              \
	wc.o

_UPROG_TEST_OBJS =    \
	forktest.o        \
	gets_test.o       \
	gets_test2.o      \
	gfx_text_test.o   \
	graphics_test.o   \
	poke_disp_test.o  \
	printf_test.o     \
	realloc_test.o    \
	shfind_test.o     \
	stackoverflow.o   \
	stressfs.o        \
	string_test.o     \
	temptest.o        \
	usertests.o       \
	vgapal_test.o     \
	zombie_test.o

_UPROG_WISC_OBJS =    \
	wisc_exec.o       \
	wisc_fault.o      \
	wisc_fmode.o      \
	wisc_fork.o       \
	wisc_fork2.o      \
	wisc_fork3.o      \
	wisc_hello.o      \
	wisc_pipe.o       \
	wisc_spinner.o    \


# --- ... -------------------------------------------------------------------

# JK... stackoverflow.com/a/4481931
KERN_OBJS       = $(addprefix $(KERNBINDIR), $(_KERN_OBJS))
ULIB_OBJS       = $(addprefix $(USERBINDIR), $(_ULIB_OBJS))
UPROG_OBJS      = $(addprefix $(USERBINDIR), $(_UPROG_OBJS))
UPROG_CORE_OBJS = $(addprefix $(USERBINDIR), $(_UPROG_CORE_OBJS))
UPROG_TEST_OBJS = $(addprefix $(USERBINDIR), $(_UPROG_TEST_OBJS))
UPROG_WISC_OBJS = $(addprefix $(USERBINDIR), $(_UPROG_WISC_OBJS))

KERN_HEADERS = $(addprefix $(KERNHEADERDIR), $(_KERN_HEADERS))
USER_HEADERS = $(addprefix $(USERHEADERDIR), $(_USER_HEADERS))


# --- ... -------------------------------------------------------------------

# JK, create directories if don't already exist
#  stackoverflow.com/a/16346321
_dummy := $(shell mkdir -p bin)
_dummy := $(shell mkdir -p bin/kern)
_dummy := $(shell mkdir -p bin/user)
_dummy := $(shell mkdir -p bin/util)
_dummy := $(shell mkdir -p debug)
_dummy := $(shell mkdir -p fs/bin)
_dummy := $(shell mkdir -p fs/dev)
_dummy := $(shell mkdir -p fs/usr/bin)
_dummy := $(shell mkdir -p fs/usr/bin/test)
_dummy := $(shell mkdir -p fs/usr/bin/wisc)
_dummy := $(shell mkdir -p img)


# ___ Configure Tools _______________________________________________________

# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf

# Using native tools (e.g., on X86 Linux)
# TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
	TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1;  \
		then echo 'i386-jos-elf-';                                                                 \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1;                                      \
		then echo '';                                                                              \
	else                                                                                           \
		echo "***" 1>&2;                                                                           \
		echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2;               \
		echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2;                      \
		echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2;                  \
		echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2;                    \
		echo "*** environment variable to that prefix and run 'make' again." 1>&2;                 \
		echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2;                      \
		echo "***" 1>&2; exit 1;                                                                   \
	fi)
endif

# If the makefile can't find QEMU, specify its path here
# QEMU = qemu-system-i386

# Try to infer the correct QEMU
ifndef QEMU
	QEMU = $(shell if which qemu > /dev/null;                                                  \
		then echo qemu; exit;                                                                  \
	elif which qemu-system-i386 > /dev/null;                                                   \
		then echo qemu-system-i386; exit;                                                      \
	elif which qemu-system-x86_64 > /dev/null;                                                 \
		then echo qemu-system-x86_64; exit;                                                    \
	else                                                                                       \
		qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu;  \
		if test -x $$qemu;                                                                     \
			then echo $$qemu; exit;                                                            \
		fi;                                                                                    \
	fi;                                                                                        \
	echo "***" 1>&2;                                                                           \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2;                           \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2;                  \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2;                  \
	echo "***" 1>&2; exit 1)
endif


CC      = $(TOOLPREFIX)gcc
AS      = $(TOOLPREFIX)gas
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump


# CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer  # JK, remove optimization

CFLAGS += -gdwarf-2  # JK, resolve gdb "can't compute CFA for this frame"
                     #     http://staff.ustc.edu.cn/~bjhua/courses/ats/2014/hw/hw-interface.html
                     #     https://forum.osdev.org/viewtopic.php?f=1&t=30570

CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
	CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
	CFLAGS += -fno-pie -nopie
endif


ASFLAGS = -m32 -gdwarf-2 -Wa,-divide


# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS = -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)


# ___ Compile images ______________________________________________________

# --- xv6.img -------------------------------------------------------------

$(IMGDIR)xv6.img: $(IMGDIR)bootblock $(IMGDIR)kernel
	dd if=/dev/zero          of=$(IMGDIR)xv6.img count=10000          # zero out 512*count bytes

	dd if=$(IMGDIR)bootblock of=$(IMGDIR)xv6.img        conv=notrunc  # first sector (512 bytes)
	dd if=$(IMGDIR)kernel    of=$(IMGDIR)xv6.img seek=1 conv=notrunc  # second sector

$(IMGDIR)xv6memfs.img: $(IMGDIR)bootblock $(IMGDIR)kernelmemfs
	dd if=/dev/zero            of=$(IMGDIR)xv6memfs.img count=10000          # zero out 512*count bytes

	dd if=$(IMGDIR)bootblock   of=$(IMGDIR)xv6memfs.img        conv=notrunc  # first sector (512 bytes)
	dd if=$(IMGDIR)kernelmemfs of=$(IMGDIR)xv6memfs.img seek=1 conv=notrunc  # second sector


$(IMGDIR)bootblock: $(KERNDIR)bootasm.S $(KERNDIR)bootmain.c   $(KERN_HEADERS)
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I $(KERNHEADERDIR) -c $(KERNDIR)bootmain.c -o $(KERNBINDIR)bootmain.o
	# $(CC) $(CFLAGS) -fno-pic -nostdinc -I $(KERNHEADERDIR) -c $(KERNDIR)bootmain.c -o $(KERNBINDIR)bootmain.o  # JK, remove optimization
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I $(KERNHEADERDIR) -c $(KERNDIR)bootasm.S -o $(KERNBINDIR)bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $(KERNBINDIR)bootblock.o $(KERNBINDIR)bootasm.o $(KERNBINDIR)bootmain.o
	$(OBJCOPY) -S -O binary -j .text $(KERNBINDIR)bootblock.o $(IMGDIR)bootblock
	$(OBJDUMP) -S -M intel $(KERNBINDIR)bootblock.o > $(DEBUGDIR)bootblock.asm
	$(KERNDIR)bootsign.pl $(IMGDIR)bootblock

$(IMGDIR)entryother: $(KERNDIR)entryother.S   $(KERN_HEADERS)
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I $(KERNHEADERDIR) -c $(KERNDIR)entryother.S -o $(KERNBINDIR)entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $(KERNBINDIR)bootblockother.o $(KERNBINDIR)entryother.o
	$(OBJCOPY) -S -O binary -j .text $(KERNBINDIR)bootblockother.o $(IMGDIR)entryother
	$(OBJDUMP) -S -M intel $(KERNBINDIR)bootblockother.o > $(DEBUGDIR)entryother.asm

$(IMGDIR)initcode: $(KERNDIR)initcode.S   $(KERN_HEADERS)
	$(CC) $(CFLAGS) -nostdinc -I $(KERNHEADERDIR) -c $(KERNDIR)initcode.S -o $(KERNBINDIR)initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(KERNBINDIR)initcode.out $(KERNBINDIR)initcode.o
	$(OBJCOPY) -S -O binary $(KERNBINDIR)initcode.out $(IMGDIR)initcode
	$(OBJDUMP) -S -M intel $(KERNBINDIR)initcode.o > $(DEBUGDIR)initcode.asm

# The kernel ELF is a composite of:
#    $(KERN_OBJS)
#    entry.o
#    entryother
#    initcode
# 
# Because of -b flag, the initcode and entryother binaries are
# placed (as is) in the .data section of the kernel ELF.
# Their contents can be found in the kernel ELF under the
# following labels:
#    _binary_img_initcode_start   .. _binary_img_initcode_end
#    _binary_img_entryother_start .. _binary_img_entryother_end
#
$(IMGDIR)kernel: $(KERN_OBJS) $(KERNBINDIR)entry.o $(IMGDIR)entryother $(IMGDIR)initcode $(KERNDIR)kernel.ld
	$(LD) $(LDFLAGS) -T $(KERNDIR)kernel.ld -o $(IMGDIR)kernel $(KERNBINDIR)entry.o $(KERN_OBJS) -b binary $(IMGDIR)initcode $(IMGDIR)entryother
	$(OBJDUMP) -S -M intel $(IMGDIR)kernel > $(DEBUGDIR)kernel.asm
	$(OBJDUMP) -D -M intel $(IMGDIR)kernel > $(DEBUGDIR)kernel_all.asm
	$(OBJDUMP) -t $(IMGDIR)kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)kernel.sym
	$(OBJDUMP) -t $(IMGDIR)kernel | sort > $(DEBUGDIR)kernel_sorted.sym

# kernelmemfs is a copy of kernel that maintains the
# disk image in memory instead of writing to a disk.
# This is not so useful for testing persistent storage or
# exploring disk buffering implementations, but it is
# great for testing the kernel on real hardware without
# needing a scratch disk.
_MEMFS_OBJS = $(filter-out ide.o, $(_KERN_OBJS)) memide.o
MEMFS_OBJS = $(addprefix $(KERNBINDIR), $(_MEMFS_OBJS))  # JK

$(IMGDIR)kernelmemfs: $(MEMFS_OBJS) $(KERNBINDIR)entry.o $(IMGDIR)entryother $(IMGDIR)initcode $(KERNDIR)kernel.ld fs.img
	$(LD) $(LDFLAGS) -T $(KERNDIR)kernel.ld -o $(IMGDIR)kernelmemfs $(KERNBINDIR)entry.o $(MEMFS_OBJS) -b binary $(IMGDIR)initcode $(IMGDIR)entryother $(IMGDIR)fs.img
	$(OBJDUMP) -S -M intel $(IMGDIR)kernelmemfs > $(DEBUGDIR)kernelmemfs.asm
	$(OBJDUMP) -t $(IMGDIR)kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)kernelmemfs.sym

# ...
$(KERNDIR)trapvectors.S: $(KERNDIR)trapvectors.pl
	$(KERNDIR)trapvectors.pl > $(KERNDIR)trapvectors.S


# http://manpages.ubuntu.com/manpages/trusty/man1/etags.xemacs21.1.html
# tags: $(_KERN_OBJS) entryother.S _init
# 	etags *.S *.c


# --- fs.img ----------------------------------------------------------------

$(UTILBINDIR)mkfs: $(SRCDIR)mkfs.c   $(KERNHEADERDIR)types.h $(KERNHEADERDIR)date.h $(KERNHEADERDIR)fs.h $(KERNHEADERDIR)stat.h $(KERNHEADERDIR)param.h
	gcc -Werror -Wall -o $(UTILBINDIR)mkfs $(SRCDIR)mkfs.c


# mkfs will clone an exisiting directory '$(FSDIR)' into 'fs.img'
# Based on:
#  https://github.com/DoctorWkt/xv6-freebsd/blob/master/Makefile
#  https://github.com/DoctorWkt/xv6-freebsd/blob/master/tools/mkfs.c
#
$(IMGDIR)fs.img: $(UTILBINDIR)mkfs $(ULIB_OBJS) $(UPROG_CORE_OBJS) $(UPROG_OBJS) $(UPROG_TEST_OBJS) $(UPROG_WISC_OBJS)

	# Keep copy up to date
	cp README $(FSDIR)

	$(UTILBINDIR)mkfs $(IMGDIR)fs.img $(FSDIR)


# JK TODO: find way to save file edits made within xv6, such
# that cleared only explicitly, instead of each time mkfs is called


# --- ... -------------------------------------------------------------------

# JK override implicit rule, so that binaries stored in BINDIR
# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $(BINDIR)$@
# %.o: %.S
# 	$(CC) $(ASFLAGS) -c $< -o $(BINDIR)$@

# JK - needed a way to trigger recompilation when a header file changes.
# There is the proper way to do it (https://stackoverflow.com/a/626299),
# and the hacky way to do it (https://stackoverflow.com/a/44022345).
# I opted for the hacky way:
#   . It works by adding the header files to a rule's prerequisites.
#   . This makes using the -I flag redundant, but leaving it in, in case decide
#     to use proper way in future
#   Pros:
#     . erm...
#   Cons:
#     . sledgehammer approach:
#       Currently adding *all* header files to each object's prerequisites,
#       regardless of whether the .c file explicitly #includes the header file...
#       This means, a change to *any* header file will trigger
#       recompilation of *all* object files.


# Used to compile kernel code -------------------------------------
$(KERNBINDIR)%.o: $(KERNDIR)%.c   $(KERN_HEADERS)
	$(CC) $(CFLAGS) -I $(KERNHEADERDIR) -c $< -o $(KERNBINDIR)$(@F)

$(KERNBINDIR)%.o: $(KERNDIR)%.S   $(KERN_HEADERS)
	$(CC) $(ASFLAGS) -I $(KERNHEADERDIR) -c $< -o $(KERNBINDIR)$(@F)


$(KERNBINDIR)trapvectors.o: $(KERNDIR)trapvectors.S
	$(CC) $(ASFLAGS) -c $< -o $(KERNBINDIR)$(@F)  # JK, stackoverflow.com/q/53348134


# Used to compile user code -------------------------------------

# Note, using '-I $(SRCDIR)' to allow kernel headers to be
# included in user programs with '#include kernel/someheader.h'
# We use this instead of '-I $(KERNHEADERDIR)' so that kernel
# headers are distinct from user headers in include statements

$(USERBINDIR)%.o: $(ULIBDIR)%.c   $(KERN_HEADERS) $(USER_HEADERS)
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)$(@F)

$(USERBINDIR)%.o: $(ULIBDIR)%.S    $(KERN_HEADERS) $(USER_HEADERS)
	$(CC) $(ASFLAGS) -I $(SRCDIR) -c $< -o $(USERBINDIR)$(@F)


$(USERBINDIR)%.o: $(UPROGCOREDIR)%.c   $(KERN_HEADERS) $(USER_HEADERS) $(ULIB_OBJS)
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)$(@F)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSBINDIR)$(*F) $(USERBINDIR)$(@F) $(ULIB_OBJS)
	$(OBJDUMP) -S -M intel $(FSBINDIR)$(*F) > $(DEBUGDIR)$(*F).asm
	$(OBJDUMP) -t $(FSBINDIR)$(*F) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$(*F).sym


$(USERBINDIR)%.o: $(UPROGAUXDIR)%.c   $(KERN_HEADERS) $(USER_HEADERS) $(ULIB_OBJS)
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)$(@F)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINDIR)$(*F) $(USERBINDIR)$(@F) $(ULIB_OBJS)
	$(OBJDUMP) -S -M intel $(FSUSRBINDIR)$(*F) > $(DEBUGDIR)$(*F).asm
	$(OBJDUMP) -t $(FSUSRBINDIR)$(*F) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$(*F).sym

$(USERBINDIR)%.o: $(UPROGAUXTESTDIR)%.c   $(KERN_HEADERS) $(USER_HEADERS) $(ULIB_OBJS)
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)$(@F)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINTESTDIR)$(*F) $(USERBINDIR)$(@F) $(ULIB_OBJS)
	$(OBJDUMP) -S -M intel $(FSUSRBINTESTDIR)$(*F) > $(DEBUGDIR)$(*F).asm
	$(OBJDUMP) -t $(FSUSRBINTESTDIR)$(*F) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$(*F).sym

$(USERBINDIR)%.o: $(UPROGAUXWISCDIR)%.c   $(KERN_HEADERS) $(USER_HEADERS) $(ULIB_OBJS)
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)$(@F)
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINWISCDIR)$(*F) $(USERBINDIR)$(@F) $(ULIB_OBJS)
	$(OBJDUMP) -S -M intel $(FSUSRBINWISCDIR)$(*F) > $(DEBUGDIR)$(*F).asm
	$(OBJDUMP) -t $(FSUSRBINWISCDIR)$(*F) | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$(*F).sym


$(USERBINDIR)forktest.o: $(UPROGAUXTESTDIR)forktest.c   $(KERNHEADERDIR)types.h $(USERHEADERDIR)user.h
	# forktest has less library code linked in
	# Needs to be small (size?) in order to be able to max out the proc table.
	# JK, added umalloc.o, hopefully nothing breaks...
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(USERHEADERDIR) -c $< -o $(USERBINDIR)forktest.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINTESTDIR)forktest $(USERBINDIR)forktest.o $(USERBINDIR)usys.o $(USERBINDIR)ulib.o $(USERBINDIR)umalloc.o
	$(OBJDUMP) -S -M intel $(FSUSRBINTESTDIR)forktest > $(DEBUGDIR)forktest.asm


# --- ? ---------------------------------------------------------------------

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.
# More details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: $(KERNBINDIR)%.o
.PRECIOUS: $(USERBINDIR)%.o
.PRECIOUS: $(UTILBINDIR)%.o
# .PRECIOUS: %.o


# --- ? ---------------------------------------------------------------------

-include *.d


# --- clean -----------------------------------------------------------------

# 'rm' does not have a "folders only" flag
# As such, for folders containing other folders, we have to
# first use 'find' to filter for files.
# Otherwise, we get the following warning:
#   "rm: cannot remove '...': Is a directory"
# https://stackoverflow.com/a/7714932

clean: 
	rm -f                   \
	$(KERNBINDIR)*          \
	$(UTILBINDIR)*          \
	$(USERBINDIR)*          \
	$(IMGDIR)*              \
	$(DEBUGDIR)*            \
	$(KERNDIR)trapvectors.S \
	$(FSBINDIR)*            \
	.gdbinit


	# Bypass rm directory warnings
	find $(FSUSRBINDIR) -type f -exec rm -f {} \;


# ___ Document ______________________________________________________________

#	# Make a printout
#	FILES = $(shell grep -v '^\#' $(DOCDIR)runoff.list)
#	PRINT =                   \
#		$(DOCDIR)runoff.list  \
#		$(DOCDIR)runoff.spec  \
#		README                \
#		$(DOCDIR)toc.hdr      \
#		$(DOCDIR)toc.ftr      \
#		$(FILES)
#	
#	xv6.pdf: $(PRINT)
#		$(DOCDIR)runoff
#		ls -l xv6.pdf
#	
#	print: xv6.pdf


# ___ Run in emulators ______________________________________________________

#	bochs: fs.img xv6.img
#		if [ ! -e .bochsrc ]; then ln -s dot-bochsrc .bochsrc; fi
#		bochs -q


# Try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb';  \
	then echo "-gdb tcp::$(GDBPORT)";                  \
else                                                   \
	echo "-s -p $(GDBPORT)";                           \
fi)

ifndef CPUS
	CPUS := 2
endif

QEMUOPTS = -smp $(CPUS) -m 512

# Use hardware virtualization if available
#  . https://github.com/SerenityOS/serenity/blob/d89cad7c571184a75a99e0be154f702f6e3510b9/Meta/run.sh#L13
#  . https://github.com/SerenityOS/serenity/blob/d89cad7c571184a75a99e0be154f702f6e3510b9/Documentation/BuildInstructions.md
#      " On Linux, QEMU is significantly faster if it's able to use KVM.
#        ... enable KVM if /dev/kvm exists and is readable+writable by the current user.
#      "
QEMUOPTS += $(shell if [ -e "/dev/kvm" ] && [ -r "/dev/kvm" ] && [ -w "/dev/kvm" ];  \
	then echo "-enable-kvm";                                                         \
fi)

QEMUOPTS_FS = -drive file=$(IMGDIR)fs.img,index=1,media=disk,format=raw -drive file=$(IMGDIR)xv6.img,index=0,media=disk,format=raw $(QEMUOPTS)

# QEMUOPTS_MEMFS = -drive file=$(IMGDIR)xv6memfs.img,index=0,media=disk,format=raw $(QEMUOPTS)

qemu: $(IMGDIR)fs.img $(IMGDIR)xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS_FS)

qemu-nox: $(IMGDIR)fs.img $(IMGDIR)xv6.img
	$(QEMU) -nographic $(QEMUOPTS_FS)

qemu-curses: $(IMGDIR)fs.img $(IMGDIR)xv6.img
	$(QEMU) -display curses $(QEMUOPTS_FS)

# qemu-memfs: $(IMGDIR)xv6memfs.img
# 	$(QEMU) $(QEMUOPTS_MEMFS)

# qemu-memfs-nox: $(IMGDIR)xv6memfs.img
# 	$(QEMU) -nographic $(QEMUOPTS_MEMFS)

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: $(IMGDIR)fs.img $(IMGDIR)xv6.img .gdbinit
	@echo "\n*** Now run 'gdb' on a separate terminal ***\n" 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS_FS) -S $(QEMUGDB)

qemu-nox-gdb: $(IMGDIR)fs.img $(IMGDIR)xv6.img .gdbinit
	@echo "\n*** Now run 'gdb' on a separate terminal ***\n" 1>&2
	$(QEMU) -nographic $(QEMUOPTS_FS) -S $(QEMUGDB)

qemu-curses-gdb: $(IMGDIR)fs.img $(IMGDIR)xv6.img .gdbinit
	@echo "\n*** Now run 'gdb' on a separate terminal ***\n" 1>&2
	$(QEMU) -display curses $(QEMUOPTS_FS) -S $(QEMUGDB)
