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
    printf("  %s            # list root directory\n", prog);
    printf("  %s /bin       # list files in /bin\n", prog);
    printf("  %s /proc      # list processes\n", prog);
}

int main(int argc, char **argv)
{
    const char *path = "/";

    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        print_help(argv[0]);
        return 0;
    }

    if (argc > 2)
    {
        printf("ls: too many arguments\n");
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    if (argc == 2)
    {
        path = argv[1];
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0)
    {
        printf("ls: cannot open '%s'\n", path);
        return 1;
    }

    struct dirent entries[64];

    /* Our current kernel getdents always returns the full listing each time,
       so we only call it once. */
    int nbytes = getdents(fd, entries, sizeof(entries));
    if (nbytes < 0)
    {
        printf("ls: getdents failed\n");
        close(fd);
        return 1;
    }

    int n = nbytes / (int) sizeof(struct dirent);

    for (int i = 0; i < n; i++)
    {
        struct dirent *de = &entries[i];

        /* Skip "." and ".." */
        if ((de->d_name[0] == '.' && de->d_name[1] == '\0') ||
            (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0'))
        {
            continue;
        }

        printf("%s\n", de->d_name);
    }

    close(fd);
    return 0;
}
