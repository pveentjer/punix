#include <stdarg.h>
#include "kernel/console.h"

struct console kconsole;

void _console_init(struct console *con)
{
    if (con && con->ops && con->ops->init)
        con->ops->init(con);
}

void console_init()
{
    console_register_vga(&kconsole);
    _console_init(&kconsole);
}

void console_clear(struct console *con)
{
    if (con && con->ops && con->ops->clear)
        con->ops->clear(con);
}

void console_put_char(struct console *con, char c)
{
    if (con && con->ops && con->ops->put_char)
        con->ops->put_char(con, c);
}

/* Helper for 64-bit division by 10 (avoids __udivdi3) */
static uint64_t udiv64_10(uint64_t n, unsigned int *rem)
{
    uint64_t q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q + (q >> 32);
    q = q >> 3;

    unsigned int r = (unsigned int)(n - q * 10);
    if (r >= 10)
    {
        q++;
        r -= 10;
    }

    *rem = r;
    return q;
}

int console_printf(struct console *con, const char *fmt, ...)
{
    int count = 0;

    if (!con || !fmt)
        return 0;

    va_list ap;
    va_start(ap, fmt);

    while (*fmt)
    {
        if (*fmt != '%')
        {
            console_put_char(con, *fmt++);
            count++;
            continue;
        }

        fmt++; /* skip '%' */
        if (!*fmt) break;

        /* Parse flags and width */
        int zero_pad = 0;
        int width = 0;

        if (*fmt == '0')
        {
            zero_pad = 1;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int is_long = 0;
        int is_long_long = 0;

        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
            if (*fmt == 'l')
            {
                is_long_long = 1;
                fmt++;
            }
        }

        switch (*fmt)
        {
            case '%':
                console_put_char(con, '%');
                count++;
                break;

            case 'c':
            {
                int c = va_arg(ap, int);
                console_put_char(con, (char)c);
                count++;
                break;
            }

            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s)
                {
                    console_put_char(con, *s++);
                    count++;
                }
                break;
            }

            case 'd':
            case 'i':
            {
                int v = va_arg(ap, int);
                unsigned int uv;
                int negative = 0;

                if (v < 0)
                {
                    negative = 1;
                    uv = (unsigned int)(-v);
                }
                else
                {
                    uv = (unsigned int)v;
                }

                char buf[16];
                int pos = 0;
                if (uv == 0)
                {
                    buf[pos++] = '0';
                }
                else
                {
                    while (uv && pos < 16)
                    {
                        buf[pos++] = '0' + (uv % 10);
                        uv /= 10;
                    }
                }

                /* Handle width and padding */
                int num_len = pos + (negative ? 1 : 0);
                int pad_len = (width > num_len) ? (width - num_len) : 0;

                if (negative)
                {
                    console_put_char(con, '-');
                    count++;
                }

                while (pad_len > 0)
                {
                    console_put_char(con, zero_pad ? '0' : ' ');
                    count++;
                    pad_len--;
                }

                while (pos > 0)
                {
                    console_put_char(con, buf[--pos]);
                    count++;
                }
                break;
            }

            case 'u':
            {
                char buf[24];
                int pos = 0;

                if (is_long_long)
                {
                    /* 64-bit unsigned */
                    uint64_t uv = va_arg(ap, uint64_t);

                    if (uv == 0)
                    {
                        buf[pos++] = '0';
                    }
                    else
                    {
                        while (uv && pos < 24)
                        {
                            unsigned int rem;
                            uv = udiv64_10(uv, &rem);
                            buf[pos++] = '0' + rem;
                        }
                    }
                }
                else
                {
                    /* 32-bit unsigned */
                    unsigned int uv = va_arg(ap, unsigned int);

                    if (uv == 0)
                    {
                        buf[pos++] = '0';
                    }
                    else
                    {
                        while (uv && pos < 24)
                        {
                            buf[pos++] = '0' + (uv % 10);
                            uv /= 10;
                        }
                    }
                }

                /* Handle width and padding */
                int pad_len = (width > pos) ? (width - pos) : 0;
                while (pad_len > 0)
                {
                    console_put_char(con, zero_pad ? '0' : ' ');
                    count++;
                    pad_len--;
                }

                while (pos > 0)
                {
                    console_put_char(con, buf[--pos]);
                    count++;
                }
                break;
            }

            case 'x':
            case 'X':
            {
                char buf[16];
                int pos = 0;
                static const char HEX[] = "0123456789ABCDEF";

                if (is_long_long)
                {
                    /* 64-bit hex */
                    uint64_t v = va_arg(ap, uint64_t);

                    if (v == 0)
                    {
                        buf[pos++] = '0';
                    }
                    else
                    {
                        while (v && pos < 16)
                        {
                            buf[pos++] = HEX[v & 0xF];
                            v >>= 4;
                        }
                    }
                }
                else
                {
                    /* 32-bit hex */
                    unsigned int v = va_arg(ap, unsigned int);

                    if (v == 0)
                    {
                        buf[pos++] = '0';
                    }
                    else
                    {
                        while (v && pos < 16)
                        {
                            buf[pos++] = HEX[v & 0xF];
                            v >>= 4;
                        }
                    }
                }

                /* Handle width and padding */
                int pad_len = (width > pos) ? (width - pos) : 0;
                while (pad_len > 0)
                {
                    console_put_char(con, zero_pad ? '0' : ' ');
                    count++;
                    pad_len--;
                }

                while (pos > 0)
                {
                    console_put_char(con, buf[--pos]);
                    count++;
                }
                break;
            }

            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                unsigned int v = (unsigned int)ptr;
                static const char HEX[] = "0123456789abcdef";

                console_put_char(con, '0');
                console_put_char(con, 'x');
                count += 2;

                for (int i = 7; i >= 0; i--)
                {
                    console_put_char(con, HEX[(v >> (i * 4)) & 0xF]);
                    count++;
                }
                break;
            }

            default:
                console_put_char(con, '%');
                console_put_char(con, *fmt);
                count += 2;
                break;
        }
        fmt++;
    }

    va_end(ap);
    return count;
}