#include "../include/kernel/libc.h"

static void print_help(const char *prog)
{
    printf("Usage: %s [args...]\n", prog);
    printf("\n");
    printf("Prints the list of arguments passed to the program.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --help    Show this help message\n");
    printf("\n");
    printf("Example:\n");
    printf("  %s foo bar baz\n", prog);
}

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--help") == 0)
    {
        print_help(argv[0]);
        return 0;
    }

    printf("Program started with %d argument%s.\n", argc, (argc == 1 ? "" : "s"));

    if (argc <= 1)
    {
        printf("No arguments provided.\n");
        return 0;
    }

    for (int i = 0; i < argc; i++)
    {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    printf("---- End of argument list ----\n");
    return 0;
}
