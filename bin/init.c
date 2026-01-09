#include <stdint.h>
#include "stdio.h"
#include "unistd.h"
#include "kernel/constants.h"

static pid_t tty_pids[TTY_COUNT];

static pid_t spawn_shell(int tty_id)
{
    pid_t pid = fork();

    if (pid < 0)
    {
        /* Fork failed */
        return pid;
    }

    if (pid == 0)
    {
        /* Child process */
        if (setctty(tty_id) < 0)
        {
            printf("setctty failed for tty%d\n", tty_id);
            exit(1);
        }

        char *sh_argv[] = {"/bin/sh", NULL};
        char *sh_envp[] = {NULL};

        if (execve("/bin/sh", sh_argv, sh_envp) < 0)
        {
            printf("execve failed for tty%d\n", tty_id);
            exit(1);
        }

        /* Never reached if execve succeeds */
    }

    /* Parent process */
    return pid;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Spawn initial shells for each TTY */
    for (int tty_id = 0; tty_id < TTY_COUNT; tty_id++)
    {
        tty_pids[tty_id] = spawn_shell(tty_id);

        if (tty_pids[tty_id] < 0)
        {
            printf("Failed to start shell for tty%d: res=%d\n", tty_id, tty_pids[tty_id]);
            tty_pids[tty_id] = 0;
        }
    }

    /* Wait and respawn loop */
    while (1)
    {
        int status;
        pid_t exited_pid = wait(&status);

        if (exited_pid < 0)
        {
            printf("wait failed %d\n", exited_pid);
            continue;
        }

        /* Find which TTY this PID belonged to and respawn */
        for (int i = 0; i < TTY_COUNT; i++)
        {
            if (tty_pids[i] == exited_pid)
            {
                tty_pids[i] = spawn_shell(i);

                if (tty_pids[i] < 0)
                {
                    printf("Failed to respawn shell for tty%d\n", i);
                    tty_pids[i] = 0;
                }

                break;
            }
        }
    }

    return 0;
}