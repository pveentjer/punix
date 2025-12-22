// kernel/sys_enter_dispatch.c (or wherever this lives)

#include <stdint.h>
#include "kernel/syscall.h"
#include "kernel/console.h"
#include "kernel/kutils.h"
#include "kernel/keyboard.h"
#include "kernel/sched.h"
#include "kernel/libc.h"
#include "kernel/dirent.h"
#include "kernel/elf_loader.h"
#include "kernel/vfs.h"
#include "kernel/mm.h"

static inline void *uaddr(uint32_t user_ptr)
{
    return (void *)user_ptr;
}

static inline const void *uaddr_ro(uint32_t user_ptr)
{
    return (const void *)uaddr(user_ptr);
}

__attribute__((used))
uint32_t sys_enter_dispatch_c(uint32_t nr,
                              uint32_t a1,
                              uint32_t a2,
                              uint32_t a3,
                              uint32_t a4,
                              uint32_t a5)
{
    (void)a5;

    switch ((enum sys_call_nr)nr)
    {
        case SYS_write:
        {
            sched_schedule();
            return (uint32_t)vfs_write((int)a1, (const char *)uaddr_ro(a2), (size_t)a3);
        }
        case SYS_read:
        {
            sched_schedule();
            return (uint32_t)vfs_read((int)a1, uaddr(a2), (size_t)a3);
        }
        case SYS_open:
        {
            return (uint32_t)vfs_open(&vfs, sched_current(),
                                      (const char *)uaddr_ro(a1),
                                      (int)a2,
                                      (int)a3);
        }
        case SYS_close:
        {
            return (uint32_t)vfs_close(&vfs, sched_current(), (int)a1);
        }
        case SYS_getdents:
        {
            return (uint32_t)vfs_getdents((int)a1, (struct dirent *)uaddr(a2), (unsigned int)a3);
        }
        case SYS_add_task:
        {
            sched_schedule();
            return (uint32_t)sched_add_task((const char *)uaddr_ro(a1),
                                            (int)a2,
                                            (char **)uaddr(a3),
                                            (char **)uaddr(a4));
        }
        case SYS_fork:
        {
            return (uint32_t)-ENOSYS;
        }
        case SYS_execve:
        {
            return (uint32_t)-ENOSYS;
        }
        case SYS_exit:
        {
            sched_exit((int)a1);
            __builtin_unreachable();
        }
        case SYS_kill:
        {
            return (uint32_t)sched_kill((pid_t)a1, (int)a2);
        }
        case SYS_waitpid:
        {
            return (uint32_t)sched_waitpid((pid_t)a1, (int *)uaddr(a2), (int)a3);
        }
        case SYS_getpid:
        {
            sched_schedule();
            return (uint32_t)sched_getpid();
        }
        case SYS_sched_yield:
        {
            sched_schedule();
            return 0;
        }
        case SYS_nice:
        {
            (void)(int)a1;
            return 0;
        }
        case SYS_brk:
        {
            return (uint32_t)mm_brk(uaddr(a1));
        }
        case SYS_chdir:
        {
            sched_schedule();
            return (uint32_t)vfs_chdir((const char *)uaddr_ro(a1));
        }
        case SYS_getcwd:
        {
            sched_schedule();
            return (uint32_t)vfs_getcwd((char *)uaddr(a1), (size_t)a2);
        }

        default:
            return (uint32_t)-ENOSYS;
    }
}
