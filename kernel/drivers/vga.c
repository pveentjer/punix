#include "../../include/kernel/vga.h"

static inline uint16_t

vga_entry(char c, uint8_t attr)
{
    return (uint16_t)
    c | ((uint16_t)
    attr << 8);
}

static volatile uint16_t *const VIDEO =
(volatile uint16_t *)VGA_TEXT_MODE_BUFFER;
static int cursor_row = 0;
static int cursor_col = 0;

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

void screen_clear(void)
{
    for (int i = 0; i < VGA_COLS * VGA_ROWS; i++)
    {
        VIDEO[i] = vga_entry(' ', VGA_ATTR_WHITE_ON_BLACK);
    }

    cursor_row = 0;
    cursor_col = 0;
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
