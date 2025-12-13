#include <stdint.h>
#include "../include/kernel/libc.h"

/* If chdir is not already declared in your libc headers, keep this. */
int chdir(const char *path);

static void print_help(const char *prog)
{
    printf("Usage: %s DIRECTORY\n", prog);
    printf("\n");
    printf("Change the current working directory to DIRECTORY.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Notes:\n");
    printf("  This is a standalone program.\n");
    printf("  Changing directory here only affects this process,\n");
    printf("  not the shell that started it.\n");
}

int main(int argc, char **argv)
{
    const char *prog = (argc > 0 && argv[0]) ? argv[0] : "cd";

    /* Help option */
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        print_help(prog);
        return 0;
    }

    if (argc < 2)
    {
        printf("%s: missing operand\n", prog);
        printf("Try '%s --help' for more information.\n", prog);
        return 1;
    }

    if (argc > 2)
    {
        printf("%s: too many arguments\n", prog);
        printf("Try '%s --help' for more information.\n", prog);
        return 1;
    }

    const char *path = argv[1];

    if (chdir(path) < 0)
    {
        printf("%s: cannot change directory to '%s'\n", prog, path);
        return 1;
    }

    return 0;
}
