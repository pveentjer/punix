#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/constants.h"

int main(int argc, char **argv)
{
    char *sh_argv[] = {"/bin/sh", NULL};
    char *sh_envp[] = {NULL};

    for (int tty_id = 0; tty_id < TTY_COUNT; tty_id++)
    {
        sched_add_task("/bin/sh", tty_id, sh_argv, sh_envp);
    }

    return 0;
}
