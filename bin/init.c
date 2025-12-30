#include <stdint.h>
#include "libc.h"
#include "stdio.h"
#include "kernel/constants.h"

int main(int argc, char **argv)
{
    char *sh_argv[] = {"/bin/sh", NULL};
    char *sh_envp[] = {NULL};

    for (int tty_id = 0; tty_id < TTY_COUNT; tty_id++)
    {
        pid_t pid = sched_add_task("/bin/sh", tty_id, sh_argv, sh_envp);

        if (pid < 0)
        {
            printf("Failed to start shell for tty_id: %d, res=%d\n", tty_id, pid);
        }
    }
    return 0;
}
