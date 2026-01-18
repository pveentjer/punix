#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#define BAD_ADDR ((void *)1)

static void show_help(const char *prog)
{
    printf("Usage: %s [user|kernel] [--help]\n", prog);
    printf("  user    trigger a user-space segfault\n");
    printf("  kernel  trigger a kernel EFAULT via write()\n");
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        show_help(argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--help") == 0)
    {
        show_help(argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "kernel") == 0)
    {
        /* Kernel: should return -EFAULT */
        return write(STDOUT_FILENO, BAD_ADDR, 10);
    }
    else if (strcmp(argv[1], "user") == 0)
    {
        /* User: should segfault */
        int x = *(volatile int *) BAD_ADDR;
        printf("%d\n", x);
        return 0;
    }
    else
    {
        show_help(argv[0]);
        return 1;
    }
}
