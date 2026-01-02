#include <stdarg.h>
#include "kernel/kutils.h"
#include "kernel/console.h"

struct console kconsole;

void _console_init(struct console *con)
{
    if (con && con->ops && con->ops->init)
    {
        con->ops->init(con);
    }
}

void console_init()
{
    console_register_vga(&kconsole);
    _console_init(&kconsole);
}

void console_clear(struct console *con)
{
    if (con && con->ops && con->ops->clear)
    {
        con->ops->clear(con);
    }
}

void console_put_char(struct console *con, char c)
{
    if (con && con->ops && con->ops->put_char)
    {
        con->ops->put_char(con, c);
    }
}

int console_printf(struct console *con, const char *fmt, ...)
{
    if (!con || !fmt)
    {
        return 0;
    }

    char buf[512];  // Temporary buffer
    va_list ap;

    va_start(ap, fmt);
    int len = k_vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* Write to console */
    for (int i = 0; i < len && i < (int)sizeof(buf) - 1; i++)
    {
        console_put_char(con, buf[i]);
    }

    return len;
}