#include <stdint.h>
#include "kernel/syscall.h"

/* ------------------------------------------------------------------
 * Syscall helpers (x86-32 Linux-style register ABI, up to 4 args)
 *
 *   EAX = nr
 *   EBX = a1
 *   ECX = a2
 *   EDX = a3
 *   ESI = a4
 *
 * Return:
 *   EAX = ret
 * ------------------------------------------------------------------ */

uint32_t __syscall0(uint32_t nr)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = 0, ecx = 0, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

uint32_t __syscall1(uint32_t nr, uint32_t a1)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = 0, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

uint32_t __syscall2(uint32_t nr, uint32_t a1, uint32_t a2)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

uint32_t __syscall3(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = 0;


    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}


uint32_t __syscall4(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = a4;

    __asm__ volatile(
            "call *%[fn]\n\t"
           : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

