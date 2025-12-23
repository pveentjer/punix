#include <stdint.h>
#include "kernel/syscall.h"
#include "include/gdt.h"   // GDT_KERNEL_DS

__attribute__((naked, used))
uint32_t sys_enter(void)
{
    __asm__ volatile(
            "pushw %%ds\n\t"
            "pushw %%es\n\t"
            "pushw %%fs\n\t"
            "pushw %%gs\n\t"

            /* load kernel data segments (preserve eax) */
            "pushl %%eax\n\t"
            "movw  %0, %%ax\n\t"
            "movw  %%ax, %%ds\n\t"
            "movw  %%ax, %%es\n\t"
            "movw  %%ax, %%fs\n\t"
            "movw  %%ax, %%gs\n\t"
            "popl  %%eax\n\t"

            /* regs -> cdecl args */
            "pushl %%edi\n\t"
            "pushl %%esi\n\t"
            "pushl %%edx\n\t"
            "pushl %%ecx\n\t"
            "pushl %%ebx\n\t"
            "pushl %%eax\n\t"
            "call  sys_enter_dispatch_c\n\t"
            "addl  $24, %%esp\n\t"

            /* restore segments */
            "popw %%gs\n\t"
            "popw %%fs\n\t"
            "popw %%es\n\t"
            "popw %%ds\n\t"
            "ret\n\t"
            :
            : "i"((uint16_t)GDT_KERNEL_DS)
            : "memory"
            );
}
