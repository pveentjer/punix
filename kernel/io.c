#include <stdint.h>
#include "vga.h"
#include "sched.h"
#include "syscalls.h"

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
    (void)buf;
    (void)count;

    switch (fd)
    {
        case FD_STDIN:
            // No keyboard/TTY implemented yet: pretend EOF
            return 0;
        case FD_STDOUT:
        case FD_STDERR:
        default:
            // Reading from these is not supported
            return -1;
    }
}

