To check out musl

git submodule add git://git.musl-libc.org/musl musl
git submodule update --init --recursive

First time build:

cmake -B build

To build and run the kernel

cmake --build build --target clean && cmake --build build --target run

The kernel will boot into a very primitive shell. Available commands

/bin/ps
/bin/loop <iterations> <indents>
/bin/test_args


To count the number of C lines:

cloc kernel/


Useful to inspect the kernel:

nm -n kernel.elf
readelf -l kernel.elf
