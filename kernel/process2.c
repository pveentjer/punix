#include <stdint.h>
#include "syscalls.h"
#include "process.h"


void process2(void)
{
    uint64_t i = 0;
    for (;;)
    {
        for (int k = 0; k < 5; k++)
        {
            printf("----Process2 run: %u\n", i++);
            delay(200000000);
        }

        yield();
    }
}

REGISTER_PROCESS(process2,"bin/process2");