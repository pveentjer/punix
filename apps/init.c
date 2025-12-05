#include <stdint.h>
#include "../include/kernel/libc.h"

/* ------------------------------------------------------------
 * Direct VGA debug helpers (bypass kernel API)
 * ------------------------------------------------------------ */

static inline volatile uint16_t *VGA = (volatile uint16_t *) 0xB8000;

static void vga_clear_row(int row, uint8_t attr)
{
    if (row < 0 || row >= 25) return;

    for (int col = 0; col < 80; col++)
    {
        uint16_t cell = ((uint16_t) attr << 8) | (uint8_t) ' ';
        VGA[row * 80 + col] = cell;
    }
}

static void vga_put_at(int row, int col, char c, uint8_t attr)
{
    uint16_t cell = ((uint16_t) attr << 8) | (uint8_t) c;
    VGA[row * 80 + col] = cell;
}

static void vga_puts_at(int row, const char *s, uint8_t attr)
{
    if (row < 0 || row >= 25) return;

    vga_clear_row(row, attr);

    int col = 0;
    while (s[col] && col < 80)
    {
        vga_put_at(row, col, s[col], attr);
        col++;
    }
}

/* handy macro so you don’t type row/attr all the time */
#define DBG(row, msg) vga_puts_at((row), (msg), 0x07)

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

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
                    { p++; }
                    if (!*p) break;

                    cmd_argv[cmd_argc++] = p;

                    while (*p && *p != ' ')
                    { p++; }
                    if (*p) *p++ = '\0';
                }

//                if (cmd_argc > 0)
//                {
//                    char *pathname = cmd_argv[0];
//                    printf("starting '%s'\n", pathname);
//
//                    // NULL-terminate argv list
//                    cmd_argv[cmd_argc] = NULL;
//
//                    int pid = fork();
//                    if (pid < 0)
//                    {
//                        printf("Failed to fork %s\n", pathname);
//                    }
//                    else if (pid == 0)
//                    {
//                        char *envp[] = {NULL};
//                        int rc = execve(pathname, cmd_argv, envp);
//
//                        // If execve returns, it failed
//                        printf("execve('%s') failed, rc=%d\n", pathname, rc);
//                        exit(1);
//                    }
//                    else
//                    {
//                        // --- Parent ---
//                        // TODO: wait for child if you implement waitpid() later
//                        printf("spawned pid=%d\n", pid);
//                    }
//                }

                if (cmd_argc > 0) {
//            DBG(25, "DBG: sched_add_task   ");
                    printf("starting '%s'\n", cmd_argv[0]);
                    sched_add_task(cmd_argv[0], cmd_argc, cmd_argv);
//            DBG(25, "DBG: after sched_add  ");
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
