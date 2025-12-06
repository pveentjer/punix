#include "../include/kernel/libc.h"
#include "../include/kernel/fcntl.h"
#include "../include/kernel/dirent.h"

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

static int is_number(const char *s)
{
    if (!s || !*s)
        return 0;

    for (const char *p = s; *p; ++p) {
        if (*p < '0' || *p > '9')
            return 0;
    }
    return 1;
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

    int fd = open("/proc", O_RDONLY, 0);
    if (fd < 0) {
        printf("ps: failed to open /proc\n");
        return 1;
    }

    printf("PID\n");

    /* Buffer for multiple dirent records */
    struct dirent buf[32];

    for (;;) {
        /* Assuming getdents returns number of BYTES read, like Linux */
        int nbytes = getdents(fd, buf, sizeof(buf));
        if (nbytes < 0) {
            printf("ps: getdents failed\n");
            break;
        }
        if (nbytes == 0) {
            /* End of directory */
            break;
        }

        int offset = 0;
        while (offset < nbytes) {
            struct dirent *de = (struct dirent *)((char *)buf + offset);

            /* Only consider directory entries with numeric names */
            if (de->d_type == DT_DIR && is_number(de->d_name)) {
                printf("%s\n", de->d_name);
            }

            /* Safety: avoid infinite loop if d_reclen is ever zero */
            if (de->d_reclen == 0) {
                break;
            }
            offset += de->d_reclen;
        }
    }

    close(fd);
    return 0;
}
