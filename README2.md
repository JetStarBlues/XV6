## Make - Nonexistent folders

When you first run `make`, **it will fail** because the following subdirectories need to be added:

```
bin/
bin/kern/
bin/util/
bin/user/

debug/

fs/
fs/bin/
fs/dev/
fs/usr/
fs/usr/bin/

img/
```

Your directory should now look like this:

```
.
├── bin
│   ├── kern
│   ├── user
│   └── util
├── debug
├── doc
├── fmt
├── fs
│   ├── bin
│   ├── dev
│   └── usr
│       └── bin
├── img
├── other
└── src
    └── usr
        ├── lib
        └── prog
            └── core
```

Ideally, instead of manually adding the folders, they would be present when you clone or download this repository. Unfortunately, I have yet to figure out how one can make Git track empty folders.


## Make - Permission Denied

When you first run `make`, you may get a `permission denied` error when `make` tries to execute the perl files `sign.pl` and `vectors.pl`. To resolve this problem, you need to give the files execute permission. This can be done as follows:

```
chmod a+x src/bootsign.pl
chmod a+x src/trapvectors.pl
```

For more information, see the discussion [here][0].


## Build and run tools

You need the following tools to build and run this project:

```
sudo apt-get install build-essential

sudo apt-get install qemu

sudo apt-get install gdb
```


## Running on 64-bit

Xv6 is 32-bit. If you are attemtping to build and run this project on a 64-bit computer, you need to create a compiler toolchain that generates 32-bit code. Please follow the detailed guide in [this link][1] for how to do so.

For the most part, you can get away with the following:

```
sudo apt-get install gcc-multilib
```





[0]: https://www.cs.bgu.ac.il/~osce151/Assignment_1?action=show-thread&id=03742be4bbf284c7dd39833c6107ab87
[1]: https://pdos.csail.mit.edu/6.828/2018/tools.html
