#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

#define VGA_TEXT_MODE_BUFFER      0xB8000u
#define VGA_COLS                  80
#define VGA_ROWS                  25
#define VGA_ATTR_WHITE_ON_BLACK   0x07u

struct console
{
    volatile uint16_t *video;

    int cols;
    int rows;

    uint8_t attr;

    int cursor_row;
    int cursor_col;
};

/* Primary kernel console instance. */
extern struct console kconsole;

/* Initialize a console instance. */
void console_init(struct console *con);

void console_clear(struct console *con);
void console_put_char(struct console *con, char c);

/*
 * printf-style function that writes to the primary kernel console.
 * (Uses kconsole internally.)
 */
int kprintf(const char *fmt, ...);

#endif /* KERNEL_CONSOLE_H */
