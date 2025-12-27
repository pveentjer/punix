#include <stdint.h>
#include "kernel/syscall.h"
#include "kernel/console.h"

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
            "movw $0x1F40, 0xB8000\n\t"   /* '@' = 0x40, attr = 0x1F */
            "addl  $24, %%esp\n\t"
            "ret\n\t"
            :
            :
            : "memory"
            );
}
