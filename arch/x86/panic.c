#include "kernel/panic.h"
#include "kernel/console.h"

void panic(char *msg)
{
    kprintf("Kernel Panic!!!\n");
    kprintf(msg);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}