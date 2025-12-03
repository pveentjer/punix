To build and run the kernel

cmake --build build --target clean && cmake --build build --target run

The kernel will boot into a very primitive shell. Available commands

/bin/ps
/bin/loop <iterations> <indents>
/bin/test_args