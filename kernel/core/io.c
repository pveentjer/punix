#include <stdint.h>
#include "../../include/kernel/vga.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/syscalls.h"
#include "../../include/kernel/keyboard.h"

ssize_t write(int fd, const void *buf, size_t count)
{
    if (buf == 0 || count == 0)
    {
        return 0;
    }

    const char *cbuf = (const char *)buf;

    switch (fd)
    {
        case FD_STDOUT:
        case FD_STDERR:
        {
            for (size_t i = 0; i < count; i++)
            {
                char c = cbuf[i];
                screen_put_char(c);
            }
            return (ssize_t)count;
        }

        case FD_STDIN:
            // Writing to stdin is not supported
            return -1;

        default:
            // Unsupported fd
            return -1;
    }
}

ssize_t read(int fd, void *buf, size_t count)
{
    if (fd != FD_STDIN)
    {
        return -1;
    }

    if (!buf || count == 0)
    {
        return 0;
    }

    char *cbuf = (char *)buf;
    size_t read_cnt = 0;

    // Non-blocking: read whatever is available, return immediately if none
    while (read_cnt < count && keyboard_has_char())
    {
        cbuf[read_cnt++] = keyboard_get_char();
    }

    return (ssize_t)read_cnt;
}
