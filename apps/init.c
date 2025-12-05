#include <stdint.h>
#include "../include/kernel/libc.h"

/* handy macro so you don’t type row/attr all the time */
#define DBG(row, msg) vga_puts_at((row), (msg), 0x07)

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

/* store PID of last created task */
static pid_t last_pid = -1;

int main(int argc, char **argv)
{
    if (argc != 1)
    {
        printf("process2: only accept 0 argument, but received %d, exiting\n", argc);
        return 1;
    }

    char line[128];
    size_t len = 0;

    char *cmd_argv[16];
    int cmd_argc = 0;

    enum cli_state state = STATE_PROMPT;

    for (;;)
    {
        switch (state)
        {
            case STATE_PROMPT:
                printf("> ");
                len = 0;
                state = STATE_READ;
                break;

            case STATE_READ:
            {
                char c;
                ssize_t n = read(FD_STDIN, &c, 1);

                if (n <= 0)
                {
                    // nothing to read this tick; just move on
                    break;
                }

                if (c == '\r' || c == '\n')
                {
                    line[len] = '\0';
                    write(FD_STDOUT, "\n", 1);
                    state = STATE_PARSE;
                    break;
                }

                if (c == '\b' || c == 127)
                {
                    if (len > 0)
                    {
                        len--;
                        write(FD_STDOUT, "\b \b", 3);
                    }
                    break;
                }

                if (len < sizeof(line) - 1)
                {
                    line[len++] = c;
                    write(FD_STDOUT, &c, 1);
                }
                break;
            }

            case STATE_PARSE:
            {
                if (len == 0)
                {
                    state = STATE_PROMPT;
                    break;
                }

                cmd_argc = 0;
                char *p = line;

                while (*p && cmd_argc < 15) // leave space for NULL terminator
                {
                    while (*p == ' ')
                        p++;
                    if (!*p) break;

                    cmd_argv[cmd_argc++] = p;

                    while (*p && *p != ' ')
                        p++;
                    if (*p) *p++ = '\0';
                }

                if (cmd_argc == 0) {
                    state = STATE_PROMPT;
                    break;
                }

                /* -------- built-in: echo (supports echo $!) -------- */
                if (strcmp(cmd_argv[0], "echo") == 0) {
                    if (cmd_argc == 1) {
                        // plain "echo"
                        printf("\n");
                    } else {
                        for (int i = 1; i < cmd_argc; i++) {
                            if (strcmp(cmd_argv[i], "$!") == 0) {
                                if (last_pid < 0) {
                                    printf("no last pid");
                                } else {
                                    printf("%d", last_pid);
                                }
                            } else {
                                printf("%s", cmd_argv[i]);
                            }
                            if (i < cmd_argc - 1)
                                printf(" ");
                        }
                        printf("\n");
                    }

                    state = STATE_PROMPT;
                    break;
                }

                /* -------- default: start a task via sched_add_task -------- */
                if (cmd_argc > 0) {
                    printf("starting '%s'\n", cmd_argv[0]);

                    // NULL-terminate argv list for the new task
                    cmd_argv[cmd_argc] = NULL;

                    pid_t pid = sched_add_task(cmd_argv[0], cmd_argc, cmd_argv);
                    if (pid < 0) {
                        printf("Failed to create task\n");
                    } else {
                        last_pid = pid;
                        printf("spawned pid=%d\n", pid);
                    }
                }

                state = STATE_PROMPT;
                break;
            }
        }

        /* cooperative scheduling: always sched_yield once per iteration */
        sched_yield();
    }

    return 0;
}
