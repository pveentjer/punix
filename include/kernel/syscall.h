#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "dirent.h"
#include "kernel/constants.h"
#include "kernel/cpu_ctx.h"


enum sys_call_nr
{
    SYS_write = 0,
    SYS_read = 1,
    SYS_open = 2,
    SYS_close = 3,
    SYS_getdents = 4,
    SYS_add_task = 5,
    SYS_fork = 6,
    SYS_execve = 7,
    SYS_exit = 8,
    SYS_kill = 9,
    SYS_waitpid = 10,
    SYS_getpid = 11,
    SYS_sched_yield = 12,
    SYS_nice = 13,
    SYS_brk = 14,
    SYS_chdir = 15,
    SYS_getcwd = 16,
    SYS_clock_gettime = 17,
};

typedef uint32_t (*sys_enter_fn_t)(void);

typedef struct kernel_entry
{
      // the function to enter the kernel
    sys_enter_fn_t sys_enter;

    struct cpu_ctx k_cpu_ctx;
};

#define KERNEL_ENTRY_ADDR       KERNEL_VA_BASE

#define KERNEL_ENTRY ((const struct kernel_entry *)KERNEL_ENTRY_ADDR)

static inline sys_enter_fn_t sys_enter_fn(void)
{
    return KERNEL_ENTRY->sys_enter;
}

#endif /* SYSCALL_H */
