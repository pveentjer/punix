#include "../../include/kernel/kernel_api.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include <stdint.h>

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2


static ssize_t k_write(int fd, const char *buf, size_t count)
{
    sched_yield();

    if (buf == 0 || count == 0)
        return 0;

    switch (fd)
    {
        case FD_STDOUT:
        case FD_STDERR:
            for (size_t i = 0; i < count; i++)
                screen_put_char(buf[i]);
            return (ssize_t) count;

        case FD_STDIN:
            return -1;

        default:
            return -1;
    }
}

static ssize_t k_read(int fd, void *buf, size_t count)
{
    sched_yield();

    if (fd != FD_STDIN)
        return -1;

    if (!buf || count == 0)
        return 0;

    char *cbuf = (char *) buf;
    size_t read_cnt = 0;

    while (read_cnt < count && keyboard_has_char())
        cbuf[read_cnt++] = keyboard_get_char();

    return (ssize_t) read_cnt;
}

static pid_t k_getpid(void)
{
    sched_yield();

    return sched_getpid();
}

static void k_yield(void)
{
    sched_yield();
}

static void k_exit(int status)
{
    sched_exit(status);
}

static void k_sched_add_task(const char *filename, int argc, char **argv)
{
    sched_yield();

    sched_add_task(filename, argc, argv);
}

static pid_t k_fork(void)
{
    sched_fork();
}

static int k_execve(const char *pathname, char *const argv[], char *const envp[])
{
    return sched_execve(pathname, argv, envp);
}

/* ------------------------------------------------------------
 * Exported API instance in its own section
 * ------------------------------------------------------------ */

__attribute__((section(".kernel_api"), used))
const struct kernel_api kernel_api_instance = {
        .write          = k_write,
        .read           = k_read,
        .getpid         = k_getpid,
        .yield          = k_yield,
        .exit           = k_exit,
        .execve         = k_execve,
        .fork           = k_fork,
        .sched_add_task = k_sched_add_task,
};
