#include <stdint.h>
#include "../../include/kernel/syscall.h"

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
static uint32_t sys_enter_dispatch(void)
{
    __asm__ volatile(
            "push %edi\n\t"        /* a5 */
            "push %esi\n\t"        /* a4 */
            "push %edx\n\t"        /* a3 */
            "push %ecx\n\t"        /* a2 */
            "push %ebx\n\t"        /* a1 */
            "push %eax\n\t"        /* nr */
            "call sys_enter_dispatch_c\n\t"
            "add  $24, %esp\n\t"   /* drop the sys call nr + 5 args from the stack */
            "ret\n\t"
            );
}

__attribute__((section(".sys_calls"), used))
const sys_enter_fn_t sys_enter_entry = sys_enter_dispatch;
