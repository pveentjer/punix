#include <stdint.h>
#include "libc.h"
#include "string.h"
#include "stdio.h"
#include "kernel/fcntl.h"
#include "dirent.h"

static void print_help(const char *prog)
{
    printf("Usage: %s [-l] [-a] [DIRECTORY]\n", prog);
    printf("\n");
    printf("List the contents of a directory.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -l        Use a long listing format\n");
    printf("  -a        Show all entries, including . and ..\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s            # list directory\n", prog);
    printf("  %s -a         # include hidden files\n", prog);
    printf("  %s -la /bin   # long list, include hidden files\n", prog);
}

static char type_char(const struct dirent *de)
{
    switch (de->d_type)
    {
        case DT_DIR:  return 'd';
        case DT_REG:  return 'f';
        default:      return '?';
    }
}

static int is_dot_entry(const struct dirent *de)
{
    return
            (de->d_name[0] == '.' && de->d_name[1] == '\0') ||
            (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0');
}

int main(int argc, char **argv)
{
    const char *path = ".";
    int long_format = 0;
    int show_all = 0;

    /* Parse arguments */
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            print_help(argv[0]);
            return 0;
        }
        else if (strcmp(argv[i], "-l") == 0)
        {
            long_format = 1;
        }
        else if (strcmp(argv[i], "-a") == 0)
        {
            show_all = 1;
        }
        else if (strcmp(argv[i], "-la") == 0 || strcmp(argv[i], "-al") == 0)
        {
            long_format = 1;
            show_all = 1;
        }
        else if (path == NULL || path == ".")
        {
            path = argv[i];
        }
        else
        {
            printf("ls: too many arguments\n");
            printf("Try '%s --help' for more information.\n", argv[0]);
            return 1;
        }
    }

    int fd = open(path, O_RDONLY, 0);
    if (fd < 0)
    {
        printf("ls: cannot open '%s'\n", path);
        return 1;
    }

    struct dirent entries[256];

    int nbytes = getdents(fd, entries, sizeof(entries));
    if (nbytes < 0)
    {
        printf("ls: getdents failed\n");
        close(fd);
        return 1;
    }

    int n = nbytes / (int)sizeof(struct dirent);

    for (int i = 0; i < n; i++)
    {
        struct dirent *de = &entries[i];

        if (!show_all && is_dot_entry(de))
        {
            continue;
        }

        if (long_format)
        {
            printf("%c %s\n", type_char(de), de->d_name);
        }
        else
        {
            printf("%s\n", de->d_name);
        }
    }

    close(fd);
    return 0;
}
