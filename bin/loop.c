#include <stdint.h>
#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"

#define MAX_INDENT 80

static void print_usage(void)
{
    printf("Usage: loop --iterations <count> --indent <spaces> [--print <0|1>]\n");
    printf("       or:   loop <iterations> <indent> [print]\n");
    printf("\n");
    printf("Options:\n");
    printf("  --iterations <count>   Number of loop iterations (> 0)\n");
    printf("  --indent <spaces>      Number of spaces to indent output (0–%d)\n", MAX_INDENT);
    printf("  --print <0|1>          Enable (1) or disable (0) printing. Default: 1\n");
    printf("  --help                 Show this help message\n");
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "--help") == 0)
    {
        print_usage();
        return 0;
    }

    int32_t iters_signed = -1;
    int32_t indent_signed = -1;
    int print_output = 1;  // default

    // Named-argument mode
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--iterations") == 0 && i + 1 < argc)
        {
            iters_signed = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--indent") == 0 && i + 1 < argc)
        {
            indent_signed = atoi(argv[++i]);
        }
        else if (strcmp(argv[i], "--print") == 0 && i + 1 < argc)
        {
            print_output = atoi(argv[++i]) != 0;
        }
    }

    // Fallback for positional arguments
    if (iters_signed < 0 && argc >= 2)
        iters_signed = atoi(argv[1]);
    if (indent_signed < 0 && argc >= 3)
        indent_signed = atoi(argv[2]);
    if (argc >= 4)
        print_output = atoi(argv[3]) != 0;

    if (iters_signed <= 0 || indent_signed < 0)
    {
        print_usage();
        return 1;
    }

    if (indent_signed > MAX_INDENT)
    {
        printf("invalid indent (max %d)\n", MAX_INDENT);
        return 1;
    }

    uint32_t iterations = (uint32_t) iters_signed;
    uint32_t indent = (uint32_t) indent_signed;

    char indent_str[MAX_INDENT + 1];
    for (uint32_t i = 0; i < indent; i++)
    {
        indent_str[i] = ' ';
    }
    indent_str[indent] = '\0';

    int pid = getpid();
    printf("iterations: %u, indent: %u, print: %d\n", iterations, indent, print_output);

    for (uint32_t i = 0; i < iterations; i++)
    {
        if (print_output)
            printf("%sProcess-%d run: %u\n", indent_str, pid, i);

        delay(200000000);
        sched_yield();
    }

    return 0;
}
