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
    yield();

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
    yield();

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
    yield();

    return getpid();
}

static void k_yield(void)
{
    yield();
}

static void k_exit(int status)
{
    exit(status);
}

static void k_sched_add_task(const char *filename, int argc, char **argv)
{
    yield();

    sched_add_task(filename, argc, argv);
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
        .sched_add_task = k_sched_add_task,
};
