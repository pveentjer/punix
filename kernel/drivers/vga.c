#include <stdarg.h>
#include "../../include/kernel/vga.h"

static inline uint16_t
vga_entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static volatile uint16_t *const VIDEO =
        (volatile uint16_t *)VGA_TEXT_MODE_BUFFER;
static int cursor_row = 0;
static int cursor_col = 0;

/* ---------------- I/O helpers ---------------- */

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

/* ---------------- Cursor update ---------------- */

static void vga_update_cursor(void)
{
    uint16_t pos = (uint16_t)(cursor_row * VGA_COLS + cursor_col);

    /* low byte */
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));

    /* high byte */
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* ---------------- Scrolling ---------------- */

static void scroll(void)
{
    // Move every row up by one
    for (int row = 0; row < VGA_ROWS - 1; row++)
    {
        for (int col = 0; col < VGA_COLS; col++)
        {
            VIDEO[row * VGA_COLS + col] =
                    VIDEO[(row + 1) * VGA_COLS + col];
        }
    }

    // Clear last row
    for (int col = 0; col < VGA_COLS; col++)
    {
        VIDEO[(VGA_ROWS - 1) * VGA_COLS + col] =
                vga_entry(' ', VGA_ATTR_WHITE_ON_BLACK);
    }

    cursor_row = VGA_ROWS - 1;
    cursor_col = 0;
}

/* ---------------- Public API ---------------- */

void screen_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
    {
        VIDEO[i] = vga_entry(' ', VGA_ATTR_WHITE_ON_BLACK);
    }

    cursor_row = 0;
    cursor_col = 0;
    vga_update_cursor();
}

void screen_put_uint64(uint64_t value)
{
    char tmp[21];
    int pos = 0;

    if (value == 0)
    {
        screen_put_char('0');
        return;
    }

    while (value > 0)
    {
        tmp[pos++] = '0' + (value % 10);
        value /= 10;
    }

    for (int i = pos - 1; i >= 0; i--)
    {
        screen_put_char(tmp[i]);
    }
}

void screen_put_char(char c)
{
    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
    }
    else if (c == '\r')
    {
        // carriage return: go to start of line
        cursor_col = 0;
    }
    else if (c == '\b')
    {
        // backspace: move left and erase
        if (cursor_col > 0)
        {
            cursor_col--;
        }
        else if (cursor_row > 0)
        {
            cursor_row--;
            cursor_col = VGA_COLS - 1;
        }
        else
        {
            // top-left already, nothing to erase
            vga_update_cursor();
            return;
        }

        VIDEO[cursor_row * VGA_COLS + cursor_col] =
                vga_entry(' ', VGA_ATTR_WHITE_ON_BLACK);
    }
    else
    {
        VIDEO[cursor_row * VGA_COLS + cursor_col] =
                vga_entry(c, VGA_ATTR_WHITE_ON_BLACK);

        cursor_col++;
        if (cursor_col >= VGA_COLS)
        {
            cursor_col = 0;
            cursor_row++;
        }
    }

    if (cursor_row >= VGA_ROWS)
    {
        scroll();
    }

    vga_update_cursor();
}

void screen_put_hex8(uint8_t v)
{
    static const char HEX[] = "0123456789ABCDEF";
    screen_put_char(HEX[v >> 4]);
    screen_put_char(HEX[v & 0x0F]);
}

void screen_print(const char *s)
{
    while (*s)
    {
        screen_put_char(*s++);
    }
}

void screen_println(const char *s)
{
    screen_print(s);
    screen_put_char('\n');
}

int screen_printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int count = 0;

    while (*fmt)
    {
        if (*fmt != '%')
        {
            screen_put_char(*fmt++);
            count++;
            continue;
        }

        fmt++;  // skip '%'
        if (*fmt == '\0')
            break;

        switch (*fmt)
        {
            case '%':
                screen_put_char('%');
                count++;
                break;

            case 'c':
            {
                int c = va_arg(ap, int);
                screen_put_char((char)c);
                count++;
                break;
            }

            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s)
                    s = "(null)";
                while (*s)
                {
                    screen_put_char(*s++);
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
                    screen_put_char('-');
                    count++;
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
                    while (uv > 0 && pos < (int)sizeof(buf))
                    {
                        buf[pos++] = '0' + (uv % 10);
                        uv /= 10;
                    }
                }
                while (pos--)
                {
                    screen_put_char(buf[pos]);
                    count++;
                }
                break;
            }

            case 'u':
            {
                unsigned int uv = va_arg(ap, unsigned int);
                char buf[16];
                int pos = 0;
                if (uv == 0)
                {
                    buf[pos++] = '0';
                }
                else
                {
                    while (uv > 0 && pos < (int)sizeof(buf))
                    {
                        buf[pos++] = '0' + (uv % 10);
                        uv /= 10;
                    }
                }
                while (pos--)
                {
                    screen_put_char(buf[pos]);
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

                if (v == 0)
                {
                    buf[pos++] = '0';
                }
                else
                {
                    while (v > 0 && pos < (int)sizeof(buf))
                    {
                        buf[pos++] = HEX[v & 0xF];
                        v >>= 4;
                    }
                }

                while (pos--)
                {
                    screen_put_char(buf[pos]);
                    count++;
                }
                break;
            }
            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                unsigned int v = (unsigned int)ptr;

                // Print "0x" prefix
                screen_put_char('0');
                screen_put_char('x');
                count += 2;

                // Print hex value (always 8 digits for pointer)
                static const char HEX[] = "0123456789abcdef";
                for (int i = 7; i >= 0; i--)
                {
                    screen_put_char(HEX[(v >> (i * 4)) & 0xF]);
                    count++;
                }
                break;
            }
            default:
                // unknown specifier, print it literally
                screen_put_char('%');
                screen_put_char(*fmt);
                count += 2;
                break;
        }

        fmt++;
    }

    va_end(ap);
    return count;
}
