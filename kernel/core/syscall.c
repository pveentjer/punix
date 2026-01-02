#include <stdint.h>
#include "kernel/syscall.h"
#include "kernel/console.h"
#include "kernel/kutils.h"
#include "kernel/keyboard.h"
#include "kernel/sched.h"
#include "dirent.h"
#include "kernel/elf_loader.h"
#include "kernel/vfs.h"
#include "kernel/mm.h"
#include "kernel/clock.h"

__attribute__((used))
__attribute__((noinline))
uint32_t sys_enter_dispatch_c(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{


//    kprintf("sys_enter_dispatch_c %u\n",nr);

    struct task *current = sched_current();
    if (current == NULL)
    {
        kprintf("No current task\n");
        return (uint32_t) -1;
    }

    switch ((enum sys_call_nr) nr)
    {
        case SYS_write:
        {
            sched_schedule();
            return (uint32_t) vfs_write((int) a1, (const char *) a2, (size_t) a3);
        }
        case SYS_read:
        {
            sched_schedule();
            return (uint32_t) vfs_read((int) a1, (void *) a2, (size_t) a3);
        }
        case SYS_open:
        {
            sched_schedule();
            return (uint32_t) vfs_open(current, (const char *) a1, (int) a2, (int) a3);
        }
        case SYS_close:
        {
            sched_schedule();
            return (uint32_t) vfs_close(current, (int) a1);
        }
        case SYS_getdents:
        {
            sched_schedule();
            return (uint32_t) vfs_getdents((int) a1, (struct dirent *) a2, (unsigned int) a3);
        }
        case SYS_add_task:
        {
            return sched_add_task((const char *) a1, (int) a2, (char **) a3, (char **) a4);
        }
        case SYS_fork:
        {
            return (uint32_t) -ENOSYS;
        }
        case SYS_execve:
        {
            return (uint32_t) -ENOSYS;
        }
        case SYS_exit:
        {
            sched_exit((int) a1);
            __builtin_unreachable();
        }
        case SYS_kill:
        {
            return (uint32_t) sched_kill((pid_t) a1, (int) a2);
        }
        case SYS_waitpid:
        {
            return (uint32_t) sched_waitpid((pid_t) a1, (int *) a2, (int) a3);
        }
        case SYS_getpid:
        {
            sched_schedule();
            return (uint32_t) sched_getpid();
        }
        case SYS_sched_yield:
        {
            sched_schedule();
            return 0;
        }
        case SYS_nice:
        {
            (void) (int) a1;
            return 0;
        }
        case SYS_brk:
        {
            sched_schedule();
            return (uint32_t) mm_brk((void *) a1);
        }
        case SYS_chdir:
        {
            sched_schedule();
            return (uint32_t) vfs_chdir((const char *) a1);
        }
        case SYS_getcwd:
        {
            sched_schedule();
            return (uint32_t) vfs_getcwd((char *) a1, (size_t) a2);
        }
        case SYS_clock_gettime:
        {
            sched_schedule();
            return (uint32_t) kclock_gettime((clockid_t) a1, (struct timespec *) a2);
        }
        default:
        {
            return (uint32_t) -ENOSYS;
        }
    }
}
