#include <stdarg.h>
#include "kernel/console.h"

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

/* Helper for 64-bit division by 10 (avoids __udivdi3) */
static uint64_t udiv64_10(uint64_t n, unsigned int *rem)
{
    /* Simple shift-and-subtract division by 10 */
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
                    pos--;
                    console_put_char(con, buf[pos]);
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

                    if (uv == 0ULL)
                    {
                        buf[pos] = '0';
                        pos++;
                    }
                    else
                    {
                        while ((uv > 0ULL) && (pos < (int)sizeof(buf)))
                        {
                            unsigned int rem;
                            uv = udiv64_10(uv, &rem);
                            buf[pos] = (char)('0' + rem);
                            pos++;
                        }
                    }
                }
                else
                {
                    /* 32-bit unsigned */
                    unsigned int uv = va_arg(ap, unsigned int);

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
                    pos--;
                    console_put_char(con, buf[pos]);
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

                    if (v == 0ULL)
                    {
                        buf[pos] = '0';
                        pos++;
                    }
                    else
                    {
                        while ((v > 0ULL) && (pos < (int)sizeof(buf)))
                        {
                            buf[pos] = HEX[v & 0xFu];
                            pos++;
                            v >>= 4;
                        }
                    }
                }
                else
                {
                    /* 32-bit hex */
                    unsigned int v = va_arg(ap, unsigned int);

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