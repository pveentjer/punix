#include "screen.h"

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static volatile uint16_t *const VIDEO = (volatile uint16_t *)VGA_TEXT_MODE_BUFFER;
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

void screen_putc(char c)
{
    if (c == '\n')
    {
        cursor_col = 0;
        cursor_row++;
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
        screen_putc(*s++);
    }
}

void screen_println(const char *s)
{
    screen_print(s);
    screen_putc('\n');
}
