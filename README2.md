## Build Tools

You need the following tools to build and run this project:

```
sudo apt update

sudo apt install build-essential

sudo apt install qemu

sudo apt install gdb
```

Note, in newer Linux distributions (for example [Ubuntu 20.04][2]), `qemu` has become a "dummy package". You can check whether QEMU has fully installed by running the command:

```
ls /usr/bin/qemu-*
```

If this fails, then run:

```
sudo apt install qemu-system
```


## Build Tools - 64-bit

If you are attemtping to build and run this project on a 64-bit computer, you also need to create a compiler toolchain that generates 32-bit code. Please follow the detailed guide in [this link][0].

For the most part, you can get away with the following:

```
sudo apt install gcc-multilib
```


## Make - Permission Denied

When you first run `make`, you may get a `permission denied` error when `make` tries to execute the perl files `bootsign.pl` and `trapvectors.pl`. To resolve this problem, you need to give the files execute permission. This can be done as follows:

```
chmod a+x src/kernel/bootsign.pl
chmod a+x src/kernel/trapvectors.pl
```

For more information, see the discussion [here][1].


## Running

There are a few ways to run the OS:

- `make qemu`
	- graphical
- `make qemu-nox`
	- text mode
- `make qemu-curses`
	- text mode displayed using curses


## Debug using GDB

1\) Run the OS using one of the following:

- `make qemu-gdb`
- `make qemu-nox-gdb`
- `make qemu-curses-gdb`

2\) Open GDB in a separate terminal.

3\) In the GDB console, type `continue`:

```
(gdb) continue
```

If you get the message `The program is not being run`, it is likely because GDB failed to load the _.gdbinit_ file in this repository. You can either:

- Add the file to your 'auto-load safe-path'. Or,
- Enter the commands in the file manually:

```
(gdb) symbol-file img/kernel
(gdb) target remote localhost:26000
(gdb) set print pretty on
(gdb) continue
```



[0]: https://pdos.csail.mit.edu/6.828/2018/tools.html
[1]: https://www.cs.bgu.ac.il/~osce151/Assignment_1?action=show-thread&id=03742be4bbf284c7dd39833c6107ab87
[2]: https://launchpad.net/ubuntu/focal/+package/qemu
