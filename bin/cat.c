#include <stdint.h>
#include "fcntl.h"
#include "string.h"
#include "stdio.h"

#define BUF_SIZE 256

static void print_help(const char *prog)
{
    printf("Usage: %s [FILE]\n", prog);
    printf("\n");
    printf("Concatenate FILE to standard output.\n");
    printf("If no file is specified, reads from standard input.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s file.txt\n", prog);
    printf("  %s < file.txt\n", prog);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        print_help(argv[0]);
        return 0;
    }

    if (argc > 2)
    {
        printf("cat: too many arguments\n");
        printf("Try '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    int fd;

    if (argc == 1)
    {
        // No file given — read from stdin
        fd = STDIN_FILENO;
    }
    else
    {
        // Open the given file
        fd = open(argv[1], O_RDONLY, 0);
        if (fd < 0)
        {
            printf("cat: cannot open '%s'\n", argv[1]);
            return 1;
        }
    }

    char buf[BUF_SIZE];
    ssize_t n;

    while ((n = read(fd, buf, BUF_SIZE)) > 0)
    {
        write(STDOUT_FILENO, buf, (size_t) n);
    }

    if (fd != STDIN_FILENO)
    {
        close(fd);
    }

    return 0;
}
