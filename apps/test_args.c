#include "../include/kernel/syscalls.h"
#include "../include/kernel/process.h"

int test_args(int argc, char **argv)
{
    printf("argc = %d\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = '%s'\n", i, argv[i]);
    }
    return 0;
}

REGISTER_PROCESS(test_args, "/bin/test_args");