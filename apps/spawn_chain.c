#include <stdint.h>
#include "../include/kernel/libc.h"

// If you don't have a header for this yet, this prototype is enough:
void sched_add_task(const char *filename, int argc, char **argv);

#define MAX_DEPTH 100000  // safety guard

/* Simple uint32 -> decimal string */
static void u32_to_str(uint32_t value, char *buf)
{
    char tmp[11]; // enough for 4294967295
    int pos = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }

    while (value > 0 && pos < 10) {
        uint32_t digit = value % 10;
        tmp[pos++] = (char)('0' + digit);
        value /= 10;
    }

    for (int i = 0; i < pos; i++) {
        buf[i] = tmp[pos - 1 - i];
    }
    buf[pos] = '\0';
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        printf("usage: spawn_chain <count>\n");
        return 1;
    }

    int32_t count_signed = atoi(argv[1]);
    if (count_signed < 0) {
        printf("invalid count (must be >= 0)\n");
        return 1;
    }
    if (count_signed > MAX_DEPTH) {
        printf("count too large (max %d)\n", MAX_DEPTH);
        return 1;
    }

    uint32_t count = (uint32_t)count_signed;
    int pid = getpid();

    printf("[pid %d] count=%u\n", pid, count);

    // Base case: no more children
    if (count == 0) {
        printf("[pid %d] leaf reached, exiting.\n", pid);
        return 0;
    }

    // Build argv for child: same program, count-1
    char next_arg[16];
    u32_to_str(count - 1, next_arg);

    // argv array for the child task: { filename, "<count-1>", NULL }
    char *child_argv[] = {
            argv[0],     // same filename as this program
            next_arg,    // new count
            NULL
    };

    printf("[pid %d] scheduling child %s %s\n", pid, argv[0], next_arg);

    // Create the next task in the chain
    sched_add_task(argv[0], 2, child_argv);

    // This task exits; kernel should call sched_exit and switch away
    printf("[pid %d] spawned child, exiting.\n", pid);
    return 0;
}
