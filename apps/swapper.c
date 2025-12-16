#include "../include/kernel/libc.h"

int main(int argc, char **argv)
{
    for(;;)
    {
        asm volatile("hlt");
        sched_yield();
    }
    return 0;
}
