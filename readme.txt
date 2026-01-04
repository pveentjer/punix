First time build:

cmake -B build

To build and run the kernel

cmake --build build --target clean && cmake --build build --target run

The kernel will boot into a very primitive shell.

Currently there are some tools in the bin directory and there is a tiny libc
implementation. But this will soon be replaced by Musl and BusyBox.

To count the number of C lines:

cloc kernel/

Useful to inspect the kernel:

nm -n kernel.elf
readelf -l kernel.elf

Useful for assembly debugging

mov word [0xB8000], 0x0740
*(volatile uint16_t*)0xB8000 = 0x1F4B;  // 'K'