#include <stdint.h>
#include "../include/kernel/libc.h"

/* ------------------------------------------------------------
 * Direct VGA debug helpers (bypass kernel API)
 * ------------------------------------------------------------ */

static inline volatile uint16_t *VGA = (volatile uint16_t *)0xB8000;

static void vga_put_at(int row, int col, char c, uint8_t attr)
{
    uint16_t cell = ((uint16_t)attr << 8) | (uint8_t)c;
    VGA[row * 80 + col] = cell;
}

static void vga_puts_at(int row, const char *s, uint8_t attr)
{
    int col = 0;
    while (s[col] && col < 80) {
        vga_put_at(row, col, s[col], attr);
        col++;
    }
}

/* handy macro so you don’t type row/attr all the time */
#define DBG(row, msg) vga_puts_at((row), (msg), 0x07)


int main(int argc, char **argv)
{
//    DBG(20, "DBG: enter main");  // bottom of screen

    if (argc != 1) {
//        DBG(21, "DBG: argc != 1");
        printf("process2: only accept 0 argument, but received %d, exiting\n", argc);
        return 1;
    }

//    DBG(21, "DBG: argc == 1");

    printf("process0: minimal CLI ready\n");

    char line[128];
    size_t len;

//    DBG(22, "DBG: enter main loop");

    for (;;) {
        printf("> ");
        len = 0;

//        DBG(23, "DBG: waiting for input");

        for (;;) {
            char c;
            ssize_t n = read(FD_STDIN, &c, 1);
            if (n <= 0) {
                yield();
                continue;
            }

            if (c == '\r' || c == '\n') {
                line[len] = '\0';
                write(FD_STDOUT, "\n", 1);
                break;
            }

            if (c == '\b' || c == 127) {
                if (len > 0) {
                    len--;
                    write(FD_STDOUT, "\b", 1);
                }
                continue;
            }

            if (len < sizeof(line) - 1) {
                line[len++] = c;
                write(FD_STDOUT, &c, 1);
            }
        }

//        DBG(23, "DBG: line complete      ");

        if (len == 0) {
//            DBG(24, "DBG: empty line");
            continue;
        }

        // Parse command line into program + args
        char *cmd_argv[16];
        int cmd_argc = 0;
        char *p = line;

        while (*p && cmd_argc < 16) {
            while (*p == ' ') p++;  // skip spaces
            if (!*p) break;

            cmd_argv[cmd_argc++] = p;

            while (*p && *p != ' ') p++;  // find end of token
            if (*p) *p++ = '\0';          // null terminate
        }

//        DBG(24, "DBG: parsed command    ");

        if (cmd_argc > 0) {
//            DBG(25, "DBG: sched_add_task   ");
            printf("starting '%s'\n", cmd_argv[0]);
            sched_add_task(cmd_argv[0], cmd_argc, cmd_argv);
//            DBG(25, "DBG: after sched_add  ");
        }
    }

    return 0;
}
