#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/process.h"

#define MAX_INDENT 80

int process1(int argc, char **argv)
{
    if (argc != 3) 
    {
        printf("usage: process1 <iterations> <indent>\n");
        return 1;
    }

    int32_t iters_signed = atoi(argv[1]);
    if (iters_signed <= 0) 
    {
        printf("invalid iteration count\n");
        return 1;
    }

    uint32_t iterations = (uint32_t)iters_signed;
    printf("iterations: %u\n", iterations);

    int32_t indent_signed = atoi(argv[2]);
    if (indent_signed < 0) 
    {
        printf("invalid indent (must be >= 0)\n");
        return 1;
    }
    if (indent_signed > MAX_INDENT) 
    {
        printf("invalid indent (max %d)\n", MAX_INDENT);
        return 1;
    }

    uint32_t indent = (uint32_t)indent_signed;

    // Build indent string
    char indent_str[MAX_INDENT + 1];
    for (uint32_t i = 0; i < indent; i++) 
    {
        indent_str[i] = ' ';
    }
    indent_str[indent] = '\0';

    int pid = getpid();

    for (uint32_t i = 0; i < iterations; i++) {
        printf("%sProcess-%d run: %u\n", indent_str, pid, i);
        delay(200000000);
        yield();
    }

    return 0;
}

REGISTER_PROCESS(process1, "/bin/loop");
