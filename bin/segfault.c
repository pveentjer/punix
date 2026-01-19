#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>

#define BAD_ADDR ((void *)1)

static void sigsegv_handler(int sig)
{
    printf("Caught signal %d (SIGSEGV)\n", sig);
    _exit(99);
}

static void show_help(const char *prog)
{
    printf("Usage: %s [user|kernel] [--handler] [--help]\n", prog);
    printf("  user       trigger a user-space segfault (default)\n");
    printf("  kernel     trigger a kernel EFAULT via write()\n");
    printf("  --handler  install custom SIGSEGV handler first\n");
}

int main(int argc, char *argv[])
{
    int use_handler = 0;
    int kernel_mode = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            show_help(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "--handler") == 0)
        {
            use_handler = 1;
        }
        else if (strcmp(argv[i], "kernel") == 0)
        {
            kernel_mode = 1;
        }
        else if (strcmp(argv[i], "user") == 0)
        {
            kernel_mode = 0;
        }
        else
        {
            show_help(argv[0]);
            return 1;
        }
    }

    if (use_handler)
    {
        struct sigaction sa;
        sa.sa_handler = sigsegv_handler;
        sa.sa_mask = 0;
        sa.sa_flags = 0;

        if (sigaction(SIGSEGV, &sa, NULL) < 0)
        {
            printf("sigaction failed\n");
            return 1;
        }
        printf("Handler installed\n");
    }

    if (kernel_mode)
    {
        return write(STDOUT_FILENO, BAD_ADDR, 10);
    }
    else
    {
        int x = *(volatile int *) BAD_ADDR;
        printf("%d\n", x);
        return 0;
    }
}