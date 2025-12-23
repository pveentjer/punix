#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "dirent.h"
#include "kernel/cpu_ctx.h"


enum sys_call_nr
{
    SYS_write = 0,
    SYS_read,
    SYS_open,
    SYS_close,
    SYS_getdents,
    SYS_add_task,
    SYS_fork,
    SYS_execve,
    SYS_exit,
    SYS_kill,
    SYS_waitpid,
    SYS_getpid,
    SYS_sched_yield,
    SYS_nice,
    SYS_brk,
    SYS_chdir,
    SYS_getcwd,
    SYS_clock_gettime,
};

typedef uint32_t (*sys_enter_fn_t)(void);

typedef struct kernel_entry
{
    // is first so that the loader can find the address of the main at
    // the beginning of the kernel memory space
    void (*kmain)(void);

    // the function to enter the kernel
    sys_enter_fn_t sys_enter;

    struct cpu_ctx k_cpu_ctx;
};

#define MB(x)                   ((x) * 1024u * 1024u)
#define KERNEL_ENTRY_ADDR    MB(1)

#define KERNEL_ENTRY ((const struct kernel_entry *)KERNEL_ENTRY_ADDR)

static inline sys_enter_fn_t sys_enter_fn(void)
{
    return KERNEL_ENTRY->sys_enter;
}

#endif /* SYSCALL_H */
