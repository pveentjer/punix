#include <stdint.h>
#include "../../include/kernel/syscall.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include "../../include/kernel/dirent.h"
#include "../../include/kernel/elf_loader.h"
#include "../../include/kernel/vfs.h"
#include "../../include/kernel/mm.h"

__attribute__((used))
static uint32_t sys_enter_dispatch_c(uint32_t nr,
                                     uint32_t a1,
                                     uint32_t a2,
                                     uint32_t a3,
                                     uint32_t a4,
                                     uint32_t a5)
{
    switch ((enum sys_call_nr)nr)
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
            return (uint32_t) vfs_open(&vfs, sched_current(), (const char *) a1, (int) a2, (int) a3);
        }
        case SYS_close:
        {
            return (uint32_t) vfs_close(&vfs, sched_current(), (int) a1);
        }
        case SYS_getdents:
        {
            return (uint32_t) vfs_getdents((int) a1, (struct dirent *) a2, (unsigned int) a3);
        }
        case SYS_add_task:
        {
            sched_schedule();
            return (uint32_t) sched_add_task((const char *) a1, (int) a2, (char **) a3, (char **) a4);
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
            return (uint32_t) 0;
        }
        case SYS_brk:
        {
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

        default:
            return (uint32_t)-ENOSYS;
    }
}

/*
 * Register ABI adapter (Linux-style x86-32):
 *   EAX = nr
 *   EBX = a1
 *   ECX = a2
 *   EDX = a3
 *   ESI = a4
 *   EDI = a5
 *
 * Converts regs -> cdecl args and calls sys_enter_dispatch_c().
 * Returns uint32_t in EAX.
 */
__attribute__((naked, used))
static uint32_t sys_enter_dispatch(void)
{
    __asm__ volatile(
            "push %edi\n\t"        /* a5 */
            "push %esi\n\t"        /* a4 */
            "push %edx\n\t"        /* a3 */
            "push %ecx\n\t"        /* a2 */
            "push %ebx\n\t"        /* a1 */
            "push %eax\n\t"        /* nr */
            "call sys_enter_dispatch_c\n\t"
            "add  $24, %esp\n\t"   /* drop the sys call nr + 5 args from the stack */
            "ret\n\t"
            );
}

__attribute__((section(".sys_calls"), used))
const sys_enter_fn_t sys_enter_entry = sys_enter_dispatch;
