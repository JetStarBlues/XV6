# Paths
SRCDIR       = src/
UHEADERDIR   = src/usr/include/
UPROGDIR     = src/usr/prog/
UPROGCOREDIR = src/usr/prog/core/
ULIBDIR      = src/usr/lib/
KERNBINDIR   = bin/kern/
UTILBINDIR   = bin/util/
USERBINDIR   = bin/user/
IMGDIR       = img/
DEBUGDIR     = debug/
DOCDIR       = doc/

FSDIR       = fs/
FSBINDIR    = fs/bin/
FSDEVDIR    = fs/dev/
FSUSRDIR    = fs/usr/
FSUSRBINDIR = fs/usr/bin/

# Kernel code
KERNOBJS =      \
	buf.o       \
	console.o   \
	debug.o     \
	display.o   \
	exec.o      \
	file.o      \
	fs.o        \
	ide.o       \
	ioapic.o    \
	kalloc.o    \
	kbd.o       \
	kprintf.o   \
	lapic.o     \
	log.o       \
	main.o      \
	mouse.o     \
	mp.o        \
	picirq.o    \
	pipe.o      \
	proc.o      \
	sleeplock.o \
	spinlock.o  \
	string.o    \
	swtch.o     \
	syscall.o   \
	sysfile.o   \
	sysmisc.o   \
	sysproc.o   \
	trapasm.o   \
	trap.o      \
	uart.o      \
	vectors.o   \
	vga.o       \
	vm.o

# User code
ULIB =         \
	GFXtext.o  \
	printf.o   \
	ulib.o     \
	umalloc.o  \
	usys.o

UPROGSCORE =        \
	cat             \
	echo            \
	grep            \
	init            \
	kill            \
	ln              \
	ls              \
	mkdir           \
	rm              \
	sh              \
	wc

UPROGS =            \
	exists          \
	forktest        \
	graphics_test   \
	gets_test       \
	gets_test2      \
	gfx_text_test   \
	hexdump         \
	keditor         \
	stackoverflow   \
	stressfs        \
	time            \
	temptest        \
	testprintf      \
	usertests       \
	wisc_exec       \
	wisc_fault      \
	wisc_fmode      \
	wisc_fork       \
	wisc_fork2      \
	wisc_fork3      \
	wisc_hello      \
	wisc_pipe       \
	wisc_spinner    \
	zombie


# JK... stackoverflow.com/a/4481931
KERNOBJS_BINS = $(addprefix $(KERNBINDIR), $(KERNOBJS))
ULIB_BINS     = $(addprefix $(USERBINDIR), $(ULIB))
UPROG_BINS    = $(addprefix $(USERBINDIR), $(UPROGS))

# Cross-compiling (e.g., on Mac OS X)
# TOOLPREFIX = i386-jos-elf

# Using native tools (e.g., on X86 Linux)
# TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if i386-jos-elf-objdump -i 2>&1 | grep '^elf32-i386$$' >/dev/null 2>&1; \
	then echo 'i386-jos-elf-'; \
	elif objdump -i 2>&1 | grep 'elf32-i386' >/dev/null 2>&1; \
	then echo ''; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find an i386-*-elf version of GCC/binutils." 1>&2; \
	echo "*** Is the directory with i386-jos-elf-gcc in your PATH?" 1>&2; \
	echo "*** If your i386-*-elf toolchain is installed with a command" 1>&2; \
	echo "*** prefix other than 'i386-jos-elf-', set your TOOLPREFIX" 1>&2; \
	echo "*** environment variable to that prefix and run 'make' again." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

# If the makefile can't find QEMU, specify its path here
# QEMU = qemu-system-i386

# Try to infer the correct QEMU
ifndef QEMU
QEMU = $(shell if which qemu > /dev/null; \
	then echo qemu; exit; \
	elif which qemu-system-i386 > /dev/null; \
	then echo qemu-system-i386; exit; \
	elif which qemu-system-x86_64 > /dev/null; \
	then echo qemu-system-x86_64; exit; \
	else \
	qemu=/Applications/Q.app/Contents/MacOS/i386-softmmu.app/Contents/MacOS/i386-softmmu; \
	if test -x $$qemu; then echo $$qemu; exit; fi; fi; \
	echo "***" 1>&2; \
	echo "*** Error: Couldn't find a working QEMU executable." 1>&2; \
	echo "*** Is the directory containing the qemu binary in your PATH" 1>&2; \
	echo "*** or have you tried setting the QEMU variable in Makefile?" 1>&2; \
	echo "***" 1>&2; exit 1)
endif

CC      = $(TOOLPREFIX)gcc
AS      = $(TOOLPREFIX)gas
LD      = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

# CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -O2 -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer
# CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer  # JK, remove optimization
CFLAGS = -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -MD -ggdb -m32 -Werror -fno-omit-frame-pointer -gdwarf-2  # JK, remove optimization, and
                                                                                                                            #     resolve gdb "can't compute CFA for this frame"
                                                                                                                            #     http://staff.ustc.edu.cn/~bjhua/courses/ats/2014/hw/hw-interface.html
                                                                                                                            #     https://forum.osdev.org/viewtopic.php?f=1&t=30570
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)
ASFLAGS = -m32 -gdwarf-2 -Wa,-divide
# FreeBSD ld wants ``elf_i386_fbsd''
LDFLAGS += -m $(shell $(LD) -V | grep elf_i386 2>/dev/null | head -n 1)

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif


# --- OS image --------------------------------------------------------------

xv6.img: bootblock kernel
	dd if=/dev/zero          of=$(IMGDIR)xv6.img count=10000          # zero out 512*count bytes

	dd if=$(IMGDIR)bootblock of=$(IMGDIR)xv6.img        conv=notrunc  # first sector (512 bytes)
	dd if=$(IMGDIR)kernel    of=$(IMGDIR)xv6.img seek=1 conv=notrunc  # second sector

xv6memfs.img: bootblock kernelmemfs
	dd if=/dev/zero            of=$(IMGDIR)xv6memfs.img count=10000          # zero out 512*count bytes

	dd if=$(IMGDIR)bootblock   of=$(IMGDIR)xv6memfs.img        conv=notrunc  # first sector (512 bytes)
	dd if=$(IMGDIR)kernelmemfs of=$(IMGDIR)xv6memfs.img seek=1 conv=notrunc  # second sector

bootblock: $(SRCDIR)bootasm.S $(SRCDIR)bootmain.c
	$(CC) $(CFLAGS) -fno-pic -O -nostdinc -I. -c $(SRCDIR)bootmain.c -o $(KERNBINDIR)bootmain.o
	# $(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c bootmain.c -o $(KERNBINDIR)bootmain.o  # JK, remove optimization
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $(SRCDIR)bootasm.S -o $(KERNBINDIR)bootasm.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 -o $(KERNBINDIR)bootblock.o $(KERNBINDIR)bootasm.o $(KERNBINDIR)bootmain.o
	$(OBJCOPY) -S -O binary -j .text $(KERNBINDIR)bootblock.o $(IMGDIR)bootblock
	$(OBJDUMP) -S -M intel $(KERNBINDIR)bootblock.o > $(DEBUGDIR)bootblock.asm
	$(SRCDIR)sign.pl $(IMGDIR)bootblock

entryother: $(SRCDIR)entryother.S
	$(CC) $(CFLAGS) -fno-pic -nostdinc -I. -c $(SRCDIR)entryother.S -o $(KERNBINDIR)entryother.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0x7000 -o $(KERNBINDIR)bootblockother.o $(KERNBINDIR)entryother.o
	$(OBJCOPY) -S -O binary -j .text $(KERNBINDIR)bootblockother.o $(IMGDIR)entryother
	$(OBJDUMP) -S -M intel $(KERNBINDIR)bootblockother.o > $(DEBUGDIR)entryother.asm

initcode: $(SRCDIR)initcode.S
	$(CC) $(CFLAGS) -nostdinc -I. -c $(SRCDIR)initcode.S -o $(KERNBINDIR)initcode.o
	$(LD) $(LDFLAGS) -N -e start -Ttext 0 -o $(KERNBINDIR)initcode.out $(KERNBINDIR)initcode.o
	$(OBJCOPY) -S -O binary $(KERNBINDIR)initcode.out $(IMGDIR)initcode
	$(OBJDUMP) -S -M intel $(KERNBINDIR)initcode.o > $(DEBUGDIR)initcode.asm

# The kernel ELF is a composite of:
#    entry.o
#    $(KERNOBJS_BINS)
#    initcode
#    entryother
# 
# Because of -b flag, the initcode and entryother binaries are
# placed (as is) in the .data section of the kernel ELF.
# Their contents can be found in the kernel ELF under the
# following labels:
#    _binary_img_initcode_start   .. _binary_img_initcode_end
#    _binary_img_entryother_start .. _binary_img_entryother_end
#
kernel: $(KERNOBJS) entry.o entryother initcode $(SRCDIR)kernel.ld
	$(LD) $(LDFLAGS) -T $(SRCDIR)kernel.ld -o $(IMGDIR)kernel $(KERNBINDIR)entry.o $(KERNOBJS_BINS) -b binary $(IMGDIR)initcode $(IMGDIR)entryother
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
MEMFSOBJS = $(filter-out ide.o, $(KERNOBJS)) memide.o
MEMFSOBJS_BINDIR = $(addprefix $(KERNBINDIR), $(MEMFSOBJS))  # JK

kernelmemfs: $(MEMFSOBJS) entry.o entryother initcode $(SRCDIR)kernel.ld fs.img
	$(LD) $(LDFLAGS) -T $(SRCDIR)kernel.ld -o $(IMGDIR)kernelmemfs $(KERNBINDIR)entry.o $(MEMFSOBJS_BINDIR) -b binary $(IMGDIR)initcode $(IMGDIR)entryother $(IMGDIR)fs.img
	$(OBJDUMP) -S -M intel $(IMGDIR)kernelmemfs > $(DEBUGDIR)kernelmemfs.asm
	$(OBJDUMP) -t $(IMGDIR)kernelmemfs | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)kernelmemfs.sym

tags: $(KERNOBJS) entryother.S _init
	etags *.S *.c

vectors.S: $(SRCDIR)vectors.pl
	$(SRCDIR)vectors.pl > $(SRCDIR)vectors.S


# --- ... -------------------------------------------------------------------

# JK override implicit rule, so that binaries stored in BINDIR
# %.o: %.c
# 	$(CC) $(CFLAGS) -c $< -o $(BINDIR)$@
# %.o: %.S
# 	$(CC) $(ASFLAGS) -c $< -o $(BINDIR)$@


# JK Used to compile kernel code
%.o: $(SRCDIR)%.c
	$(CC) $(CFLAGS) -c $< -o $(KERNBINDIR)$@
%.o: $(SRCDIR)%.S
	$(CC) $(ASFLAGS) -c $< -o $(KERNBINDIR)$@

vectors.o: vectors.S
	$(CC) $(ASFLAGS) -c $(SRCDIR)$< -o $(KERNBINDIR)$@  # JK, stackoverflow.com/q/53348134


# JK Used to compile user code
%.o: $(ULIBDIR)%.c
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(UHEADERDIR) -c $< -o $(USERBINDIR)$@
%.o: $(ULIBDIR)%.S
	$(CC) $(ASFLAGS) -I $(SRCDIR) -c $< -o $(USERBINDIR)$@

%: $(UPROGCOREDIR)%.c
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(UHEADERDIR) -c $< -o $(USERBINDIR)$*.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSBINDIR)$@ $(USERBINDIR)$*.o $(ULIB_BINS)
	$(OBJDUMP) -S -M intel $(FSBINDIR)$@ > $(DEBUGDIR)$*.asm
	$(OBJDUMP) -t $(FSBINDIR)$@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$*.sym

%: $(UPROGDIR)%.c
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(UHEADERDIR) -c $< -o $(USERBINDIR)$*.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINDIR)$@ $(USERBINDIR)$*.o $(ULIB_BINS)
	$(OBJDUMP) -S -M intel $(FSUSRBINDIR)$@ > $(DEBUGDIR)$*.asm
	$(OBJDUMP) -t $(FSUSRBINDIR)$@ | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(DEBUGDIR)$*.sym

forktest: $(UPROGDIR)forktest.c
	# forktest has less library code linked in
	# Needs to be small in order to be able to max out the proc table.
	$(CC) $(CFLAGS) -I $(SRCDIR) -I $(UHEADERDIR) -c $< -o $(USERBINDIR)forktest.o
	$(LD) $(LDFLAGS) -N -e main -Ttext 0 -o $(FSUSRBINDIR)forktest $(USERBINDIR)forktest.o $(USERBINDIR)ulib.o $(USERBINDIR)usys.o
	$(OBJDUMP) -S -M intel $(FSUSRBINDIR)forktest > $(DEBUGDIR)forktest.asm


# --- fs --------------------------------------------------------------------

mkfs: $(SRCDIR)mkfs.c $(SRCDIR)fs.h
	gcc -Werror -Wall -o $(UTILBINDIR)mkfs $(SRCDIR)mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.
# More details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: $(KERNBINDIR)%.o
.PRECIOUS: $(USERBINDIR)%.o
.PRECIOUS: $(UTILBINDIR)%.o
# .PRECIOUS: %.o

# fs.img: mkfs README $(ULIB) $(UPROGS)
# 	$(BINDIR)mkfs $(IMGDIR)fs.img README $(UPROG_BINS)

# mkfs will clone an exisiting directory (fs). Based on,
#  https://github.com/DoctorWkt/xv6-freebsd/blob/master/Makefile
#  https://github.com/DoctorWkt/xv6-freebsd/blob/master/tools/mkfs.c
#
fs.img: mkfs $(ULIB) $(UPROGS) $(UPROGSCORE)

	# Keep copy up to date
	cp README fs

	$(UTILBINDIR)mkfs $(IMGDIR)fs.img fs


# --- ? ---------------------------------------------------------------------

-include *.d


# --- clean -----------------------------------------------------------------

# TODO: once get 'make print' working, can remove associated extensions
# from 'make clean' as all will be inside 'fmt/'
clean: 
	rm -f              \
	fmt/*              \
	*.tex              \
	*.dvi              \
	*.idx              \
	*.aux              \
	*.log              \
	*.ind              \
	*.ilg              \
	$(KERNBINDIR)*     \
	$(UTILBINDIR)*     \
	$(USERBINDIR)*     \
	$(IMGDIR)*         \
	$(DEBUGDIR)*       \
	$(SRCDIR)vectors.S \
	$(FSBINDIR)*       \
	$(FSUSRBINDIR)*    \
	.gdbinit


# ___ Document ______________________________________________________________

# make a printout
FILES = $(shell grep -v '^\#' $(DOCDIR)runoff.list)
PRINT =                   \
	$(DOCDIR)runoff.list  \
	$(DOCDIR)runoff.spec  \
	README                \
	$(DOCDIR)toc.hdr      \
	$(DOCDIR)toc.ftr      \
	$(FILES)

xv6.pdf: $(PRINT)
	$(DOCDIR)runoff
	ls -l xv6.pdf

print: xv6.pdf


# ___ Run in emulators ______________________________________________________

bochs: fs.img xv6.img
	if [ ! -e .bochsrc ]; then ln -s dot-bochsrc .bochsrc; fi
	bochs -q

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)

ifndef CPUS
CPUS := 2
endif

QEMUOPTS = -drive file=$(IMGDIR)fs.img,index=1,media=disk,format=raw -drive file=$(IMGDIR)xv6.img,index=0,media=disk,format=raw -smp $(CPUS) -m 512 $(QEMUEXTRA)

qemu: fs.img xv6.img
	$(QEMU) -serial mon:stdio $(QEMUOPTS)
	# $(QEMU) -display curses $(QEMUOPTS)

qemu-nox: fs.img xv6.img
	$(QEMU) -nographic $(QEMUOPTS)

qemu-memfs: xv6memfs.img
	$(QEMU) -drive file=$(IMGDIR)xv6memfs.img,index=0,media=disk,format=raw -smp $(CPUS) -m 256

qemu-memfs-nox: xv6memfs.img
	$(QEMU) -nographic -drive file=$(IMGDIR)xv6memfs.img,index=0,media=disk,format=raw -smp $(CPUS) -m 256

.gdbinit: .gdbinit.tmpl
	sed "s/localhost:1234/localhost:$(GDBPORT)/" < $^ > $@

qemu-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -serial mon:stdio $(QEMUOPTS) -S $(QEMUGDB)

qemu-nox-gdb: fs.img xv6.img .gdbinit
	@echo "*** Now run 'gdb'." 1>&2
	$(QEMU) -nographic $(QEMUOPTS) -S $(QEMUGDB)

