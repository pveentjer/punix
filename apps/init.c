#include <stdint.h>
#include "kernel/libc.h"
#include "kernel/constants.h"

int main(int argc, char **argv)
{

    char *sh_argv[] = {"/bin/sh", NULL};
    char *sh_envp[] = {NULL};

    printf("init started\n");
    printf("TTY_COUNT %d\n",TTY_COUNT);

    for (int tty_id = 0; tty_id < TTY_COUNT; tty_id++)
    {
        printf("Starting shell on tty_id %d\n",tty_id);
        pid_t pid = sched_add_task("/bin/sh", tty_id, sh_argv, sh_envp);

        if (pid < 0)
        {
            printf("Failed to start shell\n");
        }
    }

    return 0;
}
