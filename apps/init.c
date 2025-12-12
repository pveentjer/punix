#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/constants.h"
#include "../include/kernel/sched.h"

int main(int argc, char **argv)
{
    char *sh_argv[] = {"/bin/sh", NULL};

    for (int tty_id = 0; tty_id < (int)TTY_COUNT; tty_id++)
    {
        sched_add_task("/bin/sh", 1, sh_argv, tty_id);
    }

    return 0;
}
