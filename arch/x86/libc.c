#include <stdint.h>
#include "errno.h"
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

long __syscall0(long nr)
{
    sys_enter_fn_t fn = sys_enter_fn();

    long eax = nr;
    long ebx = 0, ecx = 0, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

long __syscall1(long nr, long a1)
{
    sys_enter_fn_t fn = sys_enter_fn();

    long eax = nr;
    long ebx = a1, ecx = 0, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

long __syscall2(long nr, long a1, long a2)
{
    sys_enter_fn_t fn = sys_enter_fn();

    long eax = nr;
    long ebx = a1, ecx = a2, edx = 0, esi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

long __syscall3(long nr, long a1, long a2, long a3)
{
    sys_enter_fn_t fn = sys_enter_fn();

    long eax = nr;
    long ebx = a1, ecx = a2, edx = a3, esi = 0;


    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}


long __syscall4(long nr, long a1, long a2, long a3, long a4)
{
    sys_enter_fn_t fn = sys_enter_fn();

    long eax = nr;
    long ebx = a1, ecx = a2, edx = a3, esi = a4;

    __asm__ volatile(
            "call *%[fn]\n\t"
           : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi)
            : [fn] "r"(fn)
            : "memory", "cc", "edi"
    );

    return eax;
}

long __syscall5(long nr, long a1, long a2, long a3, long a4, long a5)
{
    return -ENOSYS;
}

long __syscall6(long nr, long a1, long a2, long a3, long a4, long a5, long a6)
{
    return -ENOSYS;
}
