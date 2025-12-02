#include "syscalls.h"
#include "process.h"
#include "sched.h"
#include "interrupt.h"

void process0(void)
{
    bool ie = interrupts_are_enabled();
    if (!ie) {
        printf("process0: interrupts NOT enabled\n");
    }

    printf("process0: minimal CLI ready\n");

    char line[128];
    size_t len;

    for (;;) {
        printf("> ");  // prompt

        len = 0;
        for (;;) {
            char c;
            ssize_t n = read(FD_STDIN, &c, 1);

            if (n <= 0) {
                // non-blocking: nothing yet, yield CPU so others can run
                yield();
                continue;
            }

            // Handle Enter
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

            // Normal printable character
            if (len < sizeof(line) - 1) {
                line[len++] = c;
                write(FD_STDOUT, &c, 1);  // echo
            }
        }

        // Empty line → just prompt again
        if (len == 0) {
            continue;
        }

        printf("starting '%s'\n", line);
        sched_add_task(line);
    }
}

REGISTER_PROCESS(process0, "bin/process0");
