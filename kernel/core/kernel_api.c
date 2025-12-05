#include "../../include/kernel/kernel_api.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include <stdint.h>

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

#define ENOSYS 38

static ssize_t sys_write(int fd, const char *buf, size_t count)
{
    sched_yield();

    if (buf == 0 || count == 0)
        return 0;

    switch (fd)
    {
        case FD_STDOUT:
        case FD_STDERR:
            for (size_t i = 0; i < count; i++)
            {
                screen_put_char(buf[i]);
            }
            return (ssize_t) count;

        case FD_STDIN:
            return -1;

        default:
            return -1;
    }
}

static ssize_t sys_read(int fd, void *buf, size_t count)
{
    sched_yield();

    if (fd != FD_STDIN)
        return -1;

    if (!buf || count == 0)
        return 0;

    char *cbuf = (char *) buf;
    size_t read_cnt = 0;

    while (read_cnt < count && keyboard_has_char())
    {
        cbuf[read_cnt++] = keyboard_get_char();
    }

    return (ssize_t) read_cnt;
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

int sys_kill(pid_t pid, int sig)
{
    return sched_kill(pid, sig);
}

int sys_nice(int inc)
{
    return -ENOSYS;
}

int sys_get_tasks(char *buf, int buf_size)
{
    return sched_get_tasks(buf, buf_size);
}

/* ------------------------------------------------------------
 * Exported API instance in its own section
 * ------------------------------------------------------------ */

__attribute__((section(".kernel_api"), used))
const struct kernel_api kernel_api_instance = {
        .sys_write          = sys_write,
        .sys_read           = sys_read,
        .sys_getpid         = sys_getpid,
        .sys_yield          = sys_yield,
        .sys_exit           = sys_exit,
        .sys_execve         = sys_execve,
        .sys_fork           = sys_fork,
        .sys_kill           = sys_kill,
        .sys_add_task       = sys_add_task,
        .sys_nice           = sys_nice,
        .sys_get_tasks      = sys_get_tasks,
};
