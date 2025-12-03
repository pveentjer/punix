#include <stdint.h>
#include "syscalls.h"
#include "process.h"
#include "interrupt.h"
#include "sched.h"

int process0(int argc, char **argv)
{
    if (argc != 1) {
        printf("process2: only accept 0 argument, but received %d, exiting\n", argc);
        return 1;
    }
    
    bool ie = interrupts_are_enabled();
    if (!ie) {
        printf("process0: interrupts NOT enabled\n");
    }
    printf("process0: minimal CLI ready\n");
    char line[128];
    size_t len;
    for (;;) {
        printf("> ");
        len = 0;
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
        if (len == 0) {
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
            if (*p) *p++ = '\0';  // null terminate
        }
        
        if (cmd_argc > 0) {
            printf("starting '%s'\n", cmd_argv[0]);
            sched_add_task(cmd_argv[0], cmd_argc, cmd_argv);
        }
    }
    
    return 0;
}
REGISTER_PROCESS(process0, "/sbin/init");