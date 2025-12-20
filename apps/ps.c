#include "kernel/libc.h"
#include "kernel/fcntl.h"
#include "kernel/dirent.h"

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

    printf("PID   COMMAND\n");

    struct dirent buf[32];

    for (;;) {
        int nbytes = getdents(fd, buf, sizeof(buf));
        if (nbytes < 0) {
            printf("ps: getdents failed\n");
            break;
        }
        if (nbytes == 0)
            break;

        int offset = 0;
        while (offset < nbytes) {
            struct dirent *de = (struct dirent *)((char *)buf + offset);

            if (de->d_type == DT_DIR && is_number(de->d_name)) {
                char comm_path[64];
                char comm[64] = {0};

                /* Build /proc/<pid>/comm */
                strcpy(comm_path, "/proc/");
                strcat(comm_path, de->d_name);
                strcat(comm_path, "/comm");

                int comm_fd = open(comm_path, O_RDONLY, 0);
                if (comm_fd >= 0) {
                    ssize_t r = read(comm_fd, comm, sizeof(comm) - 1);
                    close(comm_fd);

                    if (r > 0) {
                        if (comm[r - 1] == '\n')
                            comm[r - 1] = '\0';
                    } else {
                        strcpy(comm, "?");
                    }
                } else {
                    strcpy(comm, "?");
                }

                printf("%s %s\n", de->d_name, comm);
            }

            if (de->d_reclen == 0)
                break;
            offset += de->d_reclen;
        }
    }

    close(fd);
    return 0;
}
