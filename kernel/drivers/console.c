#include <stdarg.h>
#include "../../include/kernel/console.h"

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

/* ---------------- I/O helpers ---------------- */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

/* Primary kernel console instance. */
struct console kconsole;

/* ---------------- Cursor update ---------------- */

static void vga_update_cursor(struct console *con)
{
    if (con == NULL)
    {
        return;
    }

    int cols = con->cols;
    int row  = con->cursor_row;
    int col  = con->cursor_col;

    uint16_t pos = (uint16_t)(row * cols + col);

    /* low byte */
    outb(0x3D4u, 0x0Fu);
    outb(0x3D5u, (uint8_t)(pos & 0xFFu));

    /* high byte */
    outb(0x3D4u, 0x0Eu);
    outb(0x3D5u, (uint8_t)((pos >> 8) & 0xFFu));
}

/* ---------------- Scrolling ---------------- */

static void console_scroll(struct console *con)
{
    if (con == NULL)
    {
        return;
    }

    volatile uint16_t *video = con->video;
    int cols = con->cols;
    int rows = con->rows;
    uint8_t attr = con->attr;

    /* Move every row up by one */
    for (int row = 0; row < rows - 1; row++)
    {
        for (int col = 0; col < cols; col++)
        {
            video[row * cols + col] = video[(row + 1) * cols + col];
        }
    }

    /* Clear last row */
    for (int col = 0; col < cols; col++)
    {
        video[(rows - 1) * cols + col] = vga_entry(' ', attr);
    }

    con->cursor_row = rows - 1;
    con->cursor_col = 0;
}

/* ---------------- Public API ---------------- */

void console_init(struct console *con)
{
    if (con == NULL)
    {
        return;
    }

    con->video = (volatile uint16_t *)VGA_TEXT_MODE_BUFFER;
    con->cols  = VGA_COLS;
    con->rows  = VGA_ROWS;
    con->attr  = VGA_ATTR_WHITE_ON_BLACK;

    con->cursor_row = 0;
    con->cursor_col = 0;

    /* Clear screen on init. */
    console_clear(con);
}


void console_clear(struct console *con)
{
    if (con == NULL)
    {
        return;
    }

    volatile uint16_t *video = con->video;
    int cols = con->cols;
    int rows = con->rows;
    uint8_t attr = con->attr;

    int total = cols * rows;

    for (int i = 0; i < total; i++)
    {
        video[i] = vga_entry(' ', attr);
    }

    con->cursor_row = 0;
    con->cursor_col = 0;
    vga_update_cursor(con);
}

void console_put_char(struct console *con, char c)
{
    if (con == NULL)
    {
        return;
    }

    volatile uint16_t *video = con->video;
    int cols = con->cols;
    int rows = con->rows;
    uint8_t attr = con->attr;

    if (c == '\n')
    {
        con->cursor_col = 0;
        con->cursor_row++;
    }
    else if (c == '\r')
    {
        /* carriage return: go to start of line */
        con->cursor_col = 0;
    }
    else if (c == '\b')
    {
        /* backspace: move left and erase */
        if (con->cursor_col > 0)
        {
            con->cursor_col--;
        }
        else if (con->cursor_row > 0)
        {
            con->cursor_row--;
            con->cursor_col = cols - 1;
        }
        else
        {
            /* top-left already, nothing to erase */
            vga_update_cursor(con);
            return;
        }

        video[con->cursor_row * cols + con->cursor_col] =
                vga_entry(' ', attr);
    }
    else
    {
        video[con->cursor_row * cols + con->cursor_col] =
                vga_entry(c, attr);

        con->cursor_col++;
        if (con->cursor_col >= cols)
        {
            con->cursor_col = 0;
            con->cursor_row++;
        }
    }

    if (con->cursor_row >= rows)
    {
        console_scroll(con);
    }

    vga_update_cursor(con);
}

int console_printf(struct console *con, const char *fmt, ...)
{
    int count = 0;

    if ((con == NULL) || (fmt == NULL))
    {
        return 0;
    }

    va_list ap;
    va_start(ap, fmt);

    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            console_put_char(con, *fmt);
            fmt++;
            count++;
            continue;
        }

        fmt++;  /* skip '%' */
        if (*fmt == '\0')
        {
            break;
        }

        switch (*fmt)
        {
            case '%':
            {
                console_put_char(con, '%');
                count++;
                break;
            }

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
                if (s == NULL)
                {
                    s = "(null)";
                }
                while (*s != '\0')
                {
                    console_put_char(con, *s);
                    s++;
                    count++;
                }
                break;
            }

            case 'd':
            case 'i':
            {
                int v = va_arg(ap, int);
                unsigned int uv;

                if (v < 0)
                {
                    console_put_char(con, '-');
                    count++;
                    uv = (unsigned int)(-v);
                }
                else
                {
                    uv = (unsigned int)v;
                }

                char buf[16];
                int pos = 0;
                if (uv == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((uv > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = (char)('0' + (uv % 10u));
                        pos++;
                        uv /= 10u;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'u':
            {
                unsigned int uv = va_arg(ap, unsigned int);
                char buf[16];
                int pos = 0;

                if (uv == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((uv > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = (char)('0' + (uv % 10u));
                        pos++;
                        uv /= 10u;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'x':
            case 'X':
            {
                unsigned int v = va_arg(ap, unsigned int);
                char buf[8];
                int pos = 0;
                static const char HEX[] = "0123456789ABCDEF";

                if (v == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((v > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = HEX[v & 0xFu];
                        pos++;
                        v >>= 4;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                unsigned int v = (unsigned int)ptr;

                /* Print "0x" prefix */
                console_put_char(con, '0');
                console_put_char(con, 'x');
                count += 2;

                /* Print hex value (always 8 digits for pointer) */
                static const char HEX[] = "0123456789abcdef";
                for (int i = 7; i >= 0; i--)
                {
                    console_put_char(con, HEX[(v >> (i * 4)) & 0xFu]);
                    count++;
                }
                break;
            }

            default:
            {
                /* unknown specifier, print it literally */
                console_put_char(con, '%');
                console_put_char(con, *fmt);
                count += 2;
                break;
            }
        }

        fmt++;
    }

    va_end(ap);
    return count;
}

/* ---------------- kprintf on primary console ---------------- */

int kprintf(const char *fmt, ...)
{
    struct console *con = &kconsole;
    int count = 0;

    if ((con == NULL) || (fmt == NULL))
    {
        return 0;
    }

    va_list ap;
    va_start(ap, fmt);

    while (*fmt != '\0')
    {
        if (*fmt != '%')
        {
            console_put_char(con, *fmt);
            fmt++;
            count++;
            continue;
        }

        fmt++;  /* skip '%' */
        if (*fmt == '\0')
        {
            break;
        }

        switch (*fmt)
        {
            case '%':
            {
                console_put_char(con, '%');
                count++;
                break;
            }

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
                if (s == NULL)
                {
                    s = "(null)";
                }
                while (*s != '\0')
                {
                    console_put_char(con, *s);
                    s++;
                    count++;
                }
                break;
            }

            case 'd':
            case 'i':
            {
                int v = va_arg(ap, int);
                unsigned int uv;

                if (v < 0)
                {
                    console_put_char(con, '-');
                    count++;
                    uv = (unsigned int)(-v);
                }
                else
                {
                    uv = (unsigned int)v;
                }

                char buf[16];
                int pos = 0;
                if (uv == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((uv > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = (char)('0' + (uv % 10u));
                        pos++;
                        uv /= 10u;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'u':
            {
                unsigned int uv = va_arg(ap, unsigned int);
                char buf[16];
                int pos = 0;

                if (uv == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((uv > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = (char)('0' + (uv % 10u));
                        pos++;
                        uv /= 10u;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'x':
            case 'X':
            {
                unsigned int v = va_arg(ap, unsigned int);
                char buf[8];
                int pos = 0;
                static const char HEX[] = "0123456789ABCDEF";

                if (v == 0u)
                {
                    buf[pos] = '0';
                    pos++;
                }
                else
                {
                    while ((v > 0u) && (pos < (int)sizeof(buf)))
                    {
                        buf[pos] = HEX[v & 0xFu];
                        pos++;
                        v >>= 4;
                    }
                }

                while (pos > 0)
                {
                    pos--;
                    console_put_char(con, buf[pos]);
                    count++;
                }
                break;
            }

            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                unsigned int v = (unsigned int)ptr;

                /* Print "0x" prefix */
                console_put_char(con, '0');
                console_put_char(con, 'x');
                count += 2;

                /* Print hex value (always 8 digits for pointer) */
                static const char HEX[] = "0123456789abcdef";
                for (int i = 7; i >= 0; i--)
                {
                    console_put_char(con, HEX[(v >> (i * 4)) & 0xFu]);
                    count++;
                }
                break;
            }

            default:
            {
                /* unknown specifier, print it literally */
                console_put_char(con, '%');
                console_put_char(con, *fmt);
                count += 2;
                break;
            }
        }

        fmt++;
    }

    va_end(ap);
    return count;
}
