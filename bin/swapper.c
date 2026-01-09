#include "stdio.h"
#include "unistd.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    for(;;)
    {
        asm volatile("hlt");
        sched_yield();
    }
    return 0;
}
