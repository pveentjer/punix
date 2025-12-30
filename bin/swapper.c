#include "libc.h"

int main(int argc, char **argv)
{
//    *(volatile uint16_t*)0xB8000 = 0x1F53;  // 'S'

//    printf("swapper starting\n");

    for(;;)
    {
        asm volatile("hlt");
        sched_yield();
    }
    return 0;
}
