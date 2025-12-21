#include <stdint.h>
#include "kernel/syscall.h"

/*
 * Register ABI adapter (Linux-style x86-32):
 *   EAX = nr
 *   EBX = a1
 *   ECX = a2
 *   EDX = a3
 *   ESI = a4
 *   EDI = a5
 *
 * Converts regs -> cdecl args and calls sys_enter_dispatch_c().
 * Returns uint32_t in EAX.
 */
__attribute__((naked, used))
uint32_t sys_enter(void)
{
        __asm__ volatile(
                "push %edi\n\t"        /* a5 */
                "push %esi\n\t"        /* a4 */
                "push %edx\n\t"        /* a3 */
                "push %ecx\n\t"        /* a2 */
                "push %ebx\n\t"        /* a1 */
                "push %eax\n\t"        /* nr */
                "call sys_enter_dispatch_c\n\t"
                "add  $24, %esp\n\t"
                "ret\n\t"
                );
}
