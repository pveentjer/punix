#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/fcntl.h"
#include "../include/kernel/dirent.h"

static void print_help(const char *prog)
{
    printf("Usage: %s [DIRECTORY]\n", prog);
    printf("\n");
    printf("List the contents of a directory.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s            # list current directory\n", prog);
    printf("  %s /bin       # list files in /bin\n", prog);
}

int main(int argc, char **argv)
{
    const char *path = ".";

    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (argc > 2) {
        printf("ls: too many arguments\n");
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        path = argv[1];
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        printf("ls: cannot open '%s'\n", path);
        return 1;
    }

    struct dirent entries[64];

    for (;;) {
        int n = getdents(fd, entries, 64);
        if (n <= 0) {
            break; // 0 = EOF, <0 = error
        }

        for (int i = 0; i < n; i++) {
            struct dirent *de = &entries[i];

            // Skip "." and ".." entries
            if ((de->d_name[0] == '.' && de->d_name[1] == '\0') ||
                (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0')) {
                continue;
            }

            printf("%s\n", de->d_name);
        }
    }

    close(fd);
    return 0;
}
