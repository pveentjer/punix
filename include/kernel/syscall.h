#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "dirent.h"

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
};

typedef uint32_t (*sys_enter_fn_t)(void);

#define MB(x)               ((x) * 1024u * 1024u)
#define SYS_CALLS_HDR_ADDR  MB(1)

/*
 * Header contains a pointer to a sys_enter_fn_t variable (sys_enter_entry).
 * So: *(uint32_t*)HDR == &sys_enter_entry
 */
#define SYS_ENTER_PTRPTR_ADDR ((sys_enter_fn_t const * const *)SYS_CALLS_HDR_ADDR)

static inline sys_enter_fn_t sys_enter_fn(void)
{
    return **SYS_ENTER_PTRPTR_ADDR;  // 1) load &sys_enter_entry, 2) load sys_enter_entry (fn ptr)
}
#endif /* SYSCALL_H */
