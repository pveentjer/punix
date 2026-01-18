#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#define BAD_ADDR ((void *)1)

int main(int argc, char *argv[])
{
    if (argc != 2) {
        printf("Usage: %s [user|kernel]\n", argv[0]);
        return 1;
    }

    printf("segfault_test\n");

    if (argv[1][0] == 'k') {
        /* Kernel: should return -EFAULT */
        return write(STDOUT_FILENO, BAD_ADDR, 10);
    } else {
        /* User: should segfault */
        int x = *(volatile int *)BAD_ADDR;
        printf("%d\n", x);
        return 0;
    }
}