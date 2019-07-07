#!/bin/bash

# -- Run without compiling source --
# Not dynamic. Manually update if the commands/arguments change

if [ $# = 0 ]; then

	echo "run: No command specified"

elif [ $1 = "qemu" ]; then

	# qemu-system-i386 -serial mon:stdio -drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512

	# Text-mode only =(
	qemu-system-i386 -display curses -drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512

elif [ $1 = "qemu-nox" ]; then

	qemu-system-i386 -nographic -drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512

elif [ $1 = "qemu-nox-gdb" ]; then

	qemu-system-i386 -nographic -drive file=img/fs.img,index=1,media=disk,format=raw -drive file=img/xv6.img,index=0,media=disk,format=raw -smp 2 -m 512 -S -gdb tcp::26000

else

	echo "Command not yet implemented"

fi
