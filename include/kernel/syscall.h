#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "dirent.h"
#include "kernel/constants.h"


#define SYS_exit            1
#define SYS_fork            2
#define SYS_read            3
#define SYS_write           4
#define SYS_open            5
#define SYS_close           6
#define SYS_waitpid         7
#define SYS_execve          11
#define SYS_chdir           12
#define SYS_getpid          20
#define SYS_nice            34
#define SYS_kill            37
#define SYS_brk             45
#define SYS_getdents        141
#define SYS_sched_yield     158
#define SYS_getcwd          183
#define SYS_clock_gettime   265

// Custom syscalls (no Linux equivalent)
#define SYS_setctty   500 // Linux uses ioctl(fd, TIOCSCTTY, 0)

typedef uint32_t (*sys_enter_fn_t)(void);

typedef struct kernel_entry
{
      // the function to enter the kernel
    sys_enter_fn_t sys_enter;

};

#define KERNEL_ENTRY_ADDR       KERNEL_VA_BASE

#define KERNEL_ENTRY ((const struct kernel_entry *)KERNEL_ENTRY_ADDR)

static inline sys_enter_fn_t sys_enter_fn(void)
{
    return KERNEL_ENTRY->sys_enter;
}

#endif /* SYSCALL_H */
