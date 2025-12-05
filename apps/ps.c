#include "../include/kernel/libc.h"

static void print_help(const char *prog)
{
    printf("Usage: %s [--help]\n", prog);
    printf("\n");
    printf("List all running processes.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s\n", prog);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (argc != 1) {
        printf("ps: no arguments expected\n");
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

//    char buf[4096];
//    int n = sched_get_tasks(buf, sizeof(buf));
//
//    printf("PID   NAME\n");
//    printf("%s", buf);

    return 0;
}
