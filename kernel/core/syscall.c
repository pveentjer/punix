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
    uint32_t result;

    //kprintf("sys_enter_dispatch_c: %u\n", nr);


    struct task *current = sched_current();
    if (current == NULL)
    {
        kprintf("No current task\n");
        result = (uint32_t) -1;
        goto done;
    }

    switch ((enum sys_call_nr) nr)
    {
        case SYS_write:
            sched_schedule();
            result = (uint32_t) vfs_write((int) a1, (const char *) a2, (size_t) a3);
            break;

        case SYS_read:
            sched_schedule();
            result = (uint32_t) vfs_read((int) a1, (void *) a2, (size_t) a3);
            break;

        case SYS_open:
            sched_schedule();
            result = (uint32_t) vfs_open(current, (const char *) a1, (int) a2, (int) a3);
            break;

        case SYS_close:
            sched_schedule();
            result = (uint32_t) vfs_close(current, (int) a1);
            break;

        case SYS_getdents:
            sched_schedule();
            result = (uint32_t) vfs_getdents((int) a1, (struct dirent *) a2, (unsigned int) a3);
            break;

        case SYS_add_task:
            result = sched_add_task((const char *) a1, (int) a2, (char **) a3, (char **) a4);
            break;

        case SYS_fork:
            result = (uint32_t) -ENOSYS;
            break;

        case SYS_execve:
            result = (uint32_t) -ENOSYS;
            break;

        case SYS_exit:
            sched_exit((int) a1);
            __builtin_unreachable();

        case SYS_kill:
            result = (uint32_t) sched_kill((pid_t) a1, (int) a2);
            break;

        case SYS_waitpid:
            result = (uint32_t) sched_waitpid((pid_t) a1, (int *) a2, (int) a3);
            break;

        case SYS_getpid:
            sched_schedule();
            result = (uint32_t) sched_getpid();
            break;

        case SYS_sched_yield:
            sched_schedule();
            result = 0;
            break;

        case SYS_nice:
            result = 0;
            break;

        case SYS_brk:
            sched_schedule();
            result = (uint32_t) mm_brk((void *) a1);
            break;

        case SYS_chdir:
            sched_schedule();
            result = (uint32_t) vfs_chdir((const char *) a1);
            break;

        case SYS_getcwd:
            sched_schedule();
            result = (uint32_t) vfs_getcwd((char *) a1, (size_t) a2);
            break;

        case SYS_clock_gettime:
            sched_schedule();
            result = (uint32_t) kclock_gettime((clockid_t) a1, (struct timespec *) a2);
            break;

        default:
            result = (uint32_t) -ENOSYS;
            break;
    }

    done:
        //kprintf("sys_enter_dispatch_c: %u Done\n", nr);
        return result;
}