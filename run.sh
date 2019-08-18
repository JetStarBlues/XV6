#!/bin/bash

# -- Run without compiling source --
# Not dynamic. Manually update if the commands/arguments change

QEMUOPTS="-drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512"
QEMUGDB="-S -gdb tcp::26000"

if [ $# = 0 ]; then

	echo "run: No command specified"

elif [ $1 = "qemu" ]; then

	qemu-system-i386 -serial mon:stdio $QEMUOPTS

	# Text-mode only =(
	# qemu-system-i386 -display curses -drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512

elif [ $1 = "qemu-gdb" ]; then

	echo "Now run 'gdb' in another window"

	qemu-system-i386 -serial mon:stdio $QEMUOPTS $QEMUGDB

elif [ $1 = "qemu-nox" ]; then

	qemu-system-i386 -nographic $QEMUOPTS

elif [ $1 = "qemu-nox-gdb" ]; then

	echo "Now run 'gdb' in another window"

	qemu-system-i386 -nographic $QEMUOPTS $QEMUGDB

else

	echo "Command not yet implemented"

fi
