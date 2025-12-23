#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>
#include <stddef.h>

#define VGA_TEXT_MODE_BUFFER      0xB8000u
#define VGA_COLS                  80
#define VGA_ROWS                  25
#define VGA_ATTR_WHITE_ON_BLACK   0x07u

/*
 * Minimal escape parsing states for output.
 *
 * Supported sequences:
 *   ESC[H
 *   ESC[J     (treated as "0J")
 *   ESC[2J
 *   ESC[3J
 *
 * Anything else is ignored.
 */
enum console_esc_state
{
    CON_ESC_NONE = 0,   /* normal output */
    CON_ESC_ESC,        /* just saw ESC (0x1b) */
    CON_ESC_CSI,        /* saw ESC[ (no param yet) */

    CON_ESC_CSI_0,      /* saw ESC[0 */
    CON_ESC_CSI_2,      /* saw ESC[2 */
    CON_ESC_CSI_3,      /* saw ESC[3 */

    CON_ESC_CSI_OTHER   /* unsupported/too-long param; ignore until final byte */
};

struct console
{
    volatile uint16_t *video;

    int cols;
    int rows;

    uint8_t attr;

    int cursor_row;
    int cursor_col;

    /* Minimal escape parsing state (single field). */
    enum console_esc_state esc_state;
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
