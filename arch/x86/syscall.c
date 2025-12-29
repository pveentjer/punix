#include <stdint.h>
#include <stddef.h>
#include "kernel/syscall.h"
#include "kernel/console.h"
#include "kernel/sched.h"
#include "kernel/constants.h"

__attribute__((naked, used))
uint32_t sys_enter(void)
{

    __asm__ volatile(
        /* Save syscall args on user stack first */
            "pushl %%esi\n\t"
            "pushl %%edx\n\t"
            "pushl %%ecx\n\t"
            "pushl %%ebx\n\t"
            "pushl %%eax\n\t"

            /* Save callee-saved regs on user stack */
            "pushfl\n\t"
            "pushl %%ebp\n\t"
            "pushl %%edi\n\t"

            /* Get current task pointer and save user ESP */
            "movl %[current], %%edi\n\t"   /* edi = &sched.current */
            "movl (%%edi), %%edi\n\t"      /* edi = sched.current */
            "movl %%esp, (%%edi)\n\t"      /* task->cpu_ctx.esp = esp */

            /* Build syscall frame on kernel stack in REVERSE order for cdecl */
            "movl %[kstack], %%edi\n\t"    /* edi = kernel stack top */
            "movl 28(%%esp), %%eax\n\t"    /* get saved esi (a4) */
            "movl %%eax, -4(%%edi)\n\t"    /* write to kernel stack */
            "movl 24(%%esp), %%eax\n\t"    /* get saved edx (a3) */
            "movl %%eax, -8(%%edi)\n\t"
            "movl 20(%%esp), %%eax\n\t"    /* get saved ecx (a2) */
            "movl %%eax, -12(%%edi)\n\t"
            "movl 16(%%esp), %%eax\n\t"    /* get saved ebx (a1) */
            "movl %%eax, -16(%%edi)\n\t"
            "movl 12(%%esp), %%eax\n\t"    /* get saved eax (nr) */
            "movl %%eax, -20(%%edi)\n\t"

            /* Switch to kernel stack */
            "leal -20(%%edi), %%esp\n\t"

            /* Mark before call */
//            "movw $0x1F30, 0xB8000\n\t"  /* '0' */

            "call  sys_enter_dispatch_c\n\t"

            "addl  $20, %%esp\n\t"


            /* Restore user ESP */
            "movl %[current], %%edi\n\t"   /* edi = &sched.current */
            "movl (%%edi), %%edi\n\t"      /* edi = sched.current */
            "movl (%%edi), %%esp\n\t"      /* esp = task->cpu_ctx.esp */


            /* Mark after call */
//            "movw $0x1F31, 0xB8000\n\t"  /* '1' */

            /* Restore callee-saved */
            "popl %%edi\n\t"


            "popl %%ebp\n\t"
            "popfl\n\t"

            /* Discard saved syscall args */
            "addl $20, %%esp\n\t"

            "ret\n\t"
            :
            : [kstack] "i" (KERNEL_STACK_TOP_VA),
    [current] "m" (sched.current)
    : "memory"
    );
}