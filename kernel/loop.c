#include <stdint.h>
#include "syscalls.h"
#include "process.h"

int process1(int argc, char **argv)
{
    if (argc != 3) {
        printf("usage: process1 <iterations> <indent>\n");
        return 1;
    }
    
    int iterations = atoi(argv[1]);
    if (iterations <= 0) {
        printf("invalid iteration count\n");
        return 1;
    }

    printf("iterations: %d\n", iterations);
    
    int indent = atoi(argv[2]);
    if (indent < 0) {
        indent = 0;
    }
    

    // Build indent string
    char indent_str[256];
    for (int i = 0; i < indent && i < 255; i++) {
        indent_str[i] = ' ';
    }
    indent_str[indent < 255 ? indent : 255] = '\0';
    
    int pid = getpid();
    uint64_t i = 0;
    
    
    for (int k = 0; k < iterations; k++)
    {
        printf("%sProcess-%d run: %u\n", indent_str, pid, i++);
        delay(200000000);
        yield();
    }
}

REGISTER_PROCESS(process1, "bin/loop");