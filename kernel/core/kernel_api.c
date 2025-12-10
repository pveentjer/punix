#include <stdint.h>
#include "../../include/kernel/kernel_api.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include "../../include/kernel/dirent.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/vfs.h"


static ssize_t sys_write(int fd, const char *buf, size_t count)
{
    sched_yield();

    if (buf == NULL || count == 0)
    {
        return 0;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->fs == NULL || file->fs->write == NULL)
    {
        return -1;
    }

    return file->fs->write(file, buf, count);
}

static ssize_t sys_read(int fd, void *buf, size_t count)
{
    sched_yield();

    if (buf == NULL || count == 0)
    {
        return 0;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->fs == NULL || file->fs->read == NULL)
    {
        return -1;
    }

    return file->fs->read(file, buf, count);
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
    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->fs == NULL || file->fs->getdents == NULL)
    {
        return -1;
    }

    return file->fs->getdents(file, buf, count);
}

static pid_t sys_getpid(void)
{
    sched_yield();
    return sched_getpid();
}

static void sys_yield(void)
{
    sched_yield();
}

static void sys_exit(int status)
{
    sched_exit(status);
}

static pid_t sys_add_task(const char *filename, int argc, char **argv)
{
    sched_yield();
    return sched_add_task(filename, argc, argv);
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
    return -ENOSYS;
}


/* ------------------------------------------------------------
 * Exported API instance in its own section
 * (make sure other function pointers are set elsewhere or add them here)
 * ------------------------------------------------------------ */
__attribute__((section(".kernel_api"), used))
const struct kernel_api kernel_api_instance = {
        .sys_write      = sys_write,
        .sys_read       = sys_read,
        .sys_getpid     = sys_getpid,
        .sys_yield      = sys_yield,
        .sys_exit       = sys_exit,
        .sys_execve     = sys_execve,
        .sys_fork       = sys_fork,
        .sys_kill       = sys_kill,
        .sys_add_task   = sys_add_task,
        .sys_nice       = sys_nice,
        .sys_open       = sys_open,
        .sys_close      = sys_close,
        .sys_getdents   = sys_getdents
};
