#include <stdint.h>
#include "../include/kernel/libc.h"

int main(int argc, char **argv)
{
    if (argc > 1 && strcmp(argv[1], "--help") == 0)
    {
        printf("Usage: echo [STRING]...\n");
        printf("Echo the STRING(s) to standard output.\n");
        return 0;
    }

    for (int i = 1; i < argc; i++)
    {
        if (i > 1) printf(" ");
        printf("%s", argv[i]);
    }
    printf("\n");
    return 0;
}