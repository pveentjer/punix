#include "../include/kernel/libc.h"
#include "../include/kernel/fcntl.h"

static void print_help(const char *prog)
{
    printf("Usage: %s [--help]\n", prog);
    printf("\n");
    printf("List all running processes by reading /proc/.\n");
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

    int fd = open("/proc/", O_RDONLY, 0);   // <-- add third argument
    if (fd < 0) {
        printf("ps: failed to open /proc\n");
        return 1;
    }

    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        printf("ps: failed to read /proc\n");
        close(fd);
        return 1;
    }

    buf[n] = '\0';

    printf("PID\n");

    // Each line in /proc output assumed to be a pid
    char *p = buf;
    while (*p) {
        char *start = p;
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            *p++ = '\0';
        if (*start)
            printf("%s\n", start);
    }

    close(fd);
    return 0;
}
