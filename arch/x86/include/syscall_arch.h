#ifndef SYSCALL_ARCH_H
#define SYSCALL_ARCH_H

#include <stdint.h>

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
#define ENOSYS          38  /* Function not implemented */

typedef uint32_t (*sys_enter_fn_t)(void);

struct kernel_entry
{
    // the function to enter the kernel
    sys_enter_fn_t sys_enter;
};

#define GB(x)                   ((x) * 1024UL * 1024UL * 1024UL)
#define KERNEL_VA_BASE          GB(2)
#define KERNEL_ENTRY_ADDR       KERNEL_VA_BASE

#define KERNEL_ENTRY ((const struct kernel_entry *)KERNEL_ENTRY_ADDR)

static inline sys_enter_fn_t sys_enter_fn(void)
{
    return KERNEL_ENTRY->sys_enter;
}

static inline long __syscall0(long nr)
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

static inline long __syscall1(long nr, long a1)
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

static inline long __syscall2(long nr, long a1, long a2)
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

static inline long __syscall3(long nr, long a1, long a2, long a3)
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


static inline long __syscall4(long nr, long a1, long a2, long a3, long a4)
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

static inline long __syscall5(long nr, long a1, long a2, long a3, long a4, long a5)
{
    (void)nr;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;

    return -ENOSYS;
}

static inline long __syscall6(long nr, long a1, long a2, long a3, long a4, long a5, long a6)
{
    (void)nr;
    (void)a1;
    (void)a2;
    (void)a3;
    (void)a4;
    (void)a5;
    (void)a6;

    return -ENOSYS;
}



#define VDSO_USEFUL
#define VDSO_CGT32_SYM "__vdso_clock_gettime"
#define VDSO_CGT32_VER "LINUX_2.6"
#define VDSO_CGT_SYM "__vdso_clock_gettime64"
#define VDSO_CGT_VER "LINUX_2.6"

#endif // SYSCALL_ARCH_H