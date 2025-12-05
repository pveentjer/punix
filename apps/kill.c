#include <stdint.h>
#include "../include/kernel/libc.h"

int main(int argc, char **argv)
{
    if (argc < 2 || argc > 3)
    {
        printf("usage: kill <pid> [signal]\n");
        return 1;
    }

    int pid = atoi(argv[1]);
    if (pid <= 0)
    {
        printf("invalid pid\n");
        return 1;
    }

    // Default to signal 1 (SIG_TERM)
    int sig = 1;

    if (argc == 3)
    {
        sig = atoi(argv[2]);
        if (sig <= 0)
        {
            printf("invalid signal\n");
            return 1;
        }
    }

    int rc = kill(pid, sig);
    if (rc < 0)
    {
        printf("failed to send signal %d to pid %d\n", sig, pid);
        return 1;
    }

    printf("signal %d sent to pid %d\n", sig, pid);
    return 0;
}
