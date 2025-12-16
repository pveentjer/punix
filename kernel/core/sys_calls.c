#include <stdint.h>
#include "../../include/kernel/sys_calls.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include "../../include/kernel/dirent.h"
#include "../../include/kernel/elf_loader.h"
#include "../../include/kernel/vfs.h"
#include "../../include/kernel/mm.h"


static ssize_t sys_write(int fd, const char *buf, size_t count)
{
    sched_schedule();
    return vfs_write(fd, buf, count);
}

static ssize_t sys_read(int fd, void *buf, size_t count)
{
    sched_schedule();

    return vfs_read(fd, buf, count);
}

static int sys_open(const char *pathname, int flags, int mode)
{
    return vfs_open(&vfs, sched_current(), pathname, flags, mode);
}

static int sys_close(int fd)
{
    return vfs_close(&vfs, sched_current(), fd);
}

static int sys_getdents(int fd, struct dirent *buf, unsigned int count)
{
    return vfs_getdents(fd, buf, count);
}

static pid_t sys_getpid(void)
{
    sched_schedule();
    return sched_getpid();
}

static void sys_sched_yield(void)
{
    sched_schedule();
}

static void sys_exit(int status)
{
    sched_exit(status);
}

static pid_t sys_add_task(const char *filename, int tty_id, char **argv, char **envp)
{
    sched_schedule();
    return sched_add_task(filename, tty_id, argv, envp);
}

static pid_t sys_fork(void)
{
    return -ENOSYS;
}

static int sys_execve(const char *pathname, char *const argv[], char *const envp[])
{
    return -ENOSYS;
}

static int sys_kill(pid_t pid, int sig)
{
    return sched_kill(pid, sig);
}

static int sys_nice(int inc)
{
    return 0;   // do nothing, succeed
}

static pid_t sys_waitpid(pid_t pid, int *status, int options)
{
    return sched_waitpid(pid, status, options);
}

static int sys_brk(void *addr)
{
    return mm_brk(addr);
}

static int sys_chdir(const char *path)
{
    sched_schedule();

    return vfs_chdir(path);
}

static char *sys_getcwd(char *buf, size_t size)
{
    sched_schedule();

    return vfs_getcwd(buf, size);
}

static uint32_t sys_enter_dispatch(uint32_t nr,
                                   uint32_t a1,
                                   uint32_t a2,
                                   uint32_t a3,
                                   uint32_t a4)
{
    switch ((enum sys_call_nr)nr)
    {
        case SYS_write:
            return (uint32_t)sys_write((int)a1, (const char *)a2, (size_t)a3);

        case SYS_read:
            return (uint32_t)sys_read((int)a1, (void *)a2, (size_t)a3);

        case SYS_open:
            return (uint32_t)sys_open((const char *)a1, (int)a2, (int)a3);

        case SYS_close:
            return (uint32_t)sys_close((int)a1);

        case SYS_getdents:
            return (uint32_t)sys_getdents((int)a1,
                                          (struct dirent *)a2,
                                          (unsigned int)a3);

        case SYS_add_task:
            return (uint32_t)sys_add_task((const char *)a1,
                                          (int)a2,
                                          (char **)a3,
                                          (char **)a4);

        case SYS_fork:
            return (uint32_t)sys_fork();

        case SYS_execve:
            return (uint32_t)sys_execve((const char *)a1,
                                        (char *const *)a2,
                                        (char *const *)a3);

        case SYS_exit:
            sys_exit((int)a1);
            __builtin_unreachable();

        case SYS_kill:
            return (uint32_t)sys_kill((pid_t)a1, (int)a2);

        case SYS_waitpid:
            return (uint32_t)sys_waitpid((pid_t)a1, (int *)a2, (int)a3);

        case SYS_getpid:
            return (uint32_t)sys_getpid();

        case SYS_sched_yield:
            sys_sched_yield();
            return 0;

        case SYS_nice:
            return (uint32_t)sys_nice((int)a1);

        case SYS_brk:
            return (uint32_t)sys_brk((void *)a1);

        case SYS_chdir:
            return (uint32_t)sys_chdir((const char *)a1);

        case SYS_getcwd:
            return (uint32_t)sys_getcwd((char *)a1, (size_t)a2);

        default:
            return (uint32_t)-ENOSYS;
    }
}


__attribute__((section(".sys_calls"), used))
const struct sys_calls sys_call_instance = {
        .write              = sys_write,
        .read               = sys_read,
        .getpid             = sys_getpid,
        .sched_yield        = sys_sched_yield,
        .exit               = sys_exit,
        .execve             = sys_execve,
        .fork               = sys_fork,
        .kill               = sys_kill,
        .add_task           = sys_add_task,
        .nice               = sys_nice,
        .open               = sys_open,
        .close              = sys_close,
        .getdents           = sys_getdents,
        .waitpid            = sys_waitpid,
        .brk                = sys_brk,
        .chdir              = sys_chdir,
        .getcwd             = sys_getcwd,
        .sys_enter_fn       = sys_enter_dispatch,
};
