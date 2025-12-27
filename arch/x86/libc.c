#include <stdint.h>
#include "kernel/syscall.h"

/* ------------------------------------------------------------------
 * Syscall helpers (x86-32 Linux-style register ABI, up to 5 args)
 *
 *   EAX = nr
 *   EBX = a1
 *   ECX = a2
 *   EDX = a3
 *   ESI = a4
 *   EDI = a5
 *
 * Return:
 *   EAX = ret
 * ------------------------------------------------------------------ */

uint32_t syscall_0(uint32_t nr)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = 0, ecx = 0, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

uint32_t syscall_1(uint32_t nr, uint32_t a1)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = 0, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

uint32_t syscall_2(uint32_t nr, uint32_t a1, uint32_t a2)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline void vga_put_hex32_at(int col, uint32_t v)
{
    volatile uint16_t *vga = (volatile uint16_t *)0xB8000 + col;
    for (int i = 0; i < 8; i++)
    {
        uint8_t n = (v >> 28) & 0xF;
        char c = (n < 10) ? ('0' + n) : ('A' + (n - 10));
        vga[i] = (uint16_t)c | 0x0700;
        v <<= 4;
    }
}

uint32_t syscall_3(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = 0, edi = 0;


    /* --- REAL SYSCALL --- */
    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

//    *(volatile uint16_t*)0xB8000 = 0x1F51;  // 'Q'

    return eax;
}


uint32_t syscall_4(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = a4, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

uint32_t syscall_5(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = a4, edi = a5;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}
