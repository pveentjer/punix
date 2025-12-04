#include "../include/kernel/libc.h"

int main(int argc, char **argv)
{
    printf("Program started with %d argument%s.\n", argc, (argc == 1 ? "" : "s"));

    if (argc == 0) {
        printf("No arguments provided.\n");
        return 0;
    }

    for (int i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    printf("---- End of argument list ----\n");
    return 0;
}
