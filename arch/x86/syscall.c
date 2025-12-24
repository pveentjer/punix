#include <stdint.h>
#include "kernel/syscall.h"

__attribute__((naked, used))
uint32_t sys_enter(void)
{
    __asm__ volatile(
        /* regs -> cdecl args */
            "pushl %%edi\n\t"
            "pushl %%esi\n\t"
            "pushl %%edx\n\t"
            "pushl %%ecx\n\t"
            "pushl %%ebx\n\t"
            "pushl %%eax\n\t"
            "call  sys_enter_dispatch_c\n\t"
            "addl  $24, %%esp\n\t"
            "ret\n\t"
            :
            :
            : "memory"
            );
}
