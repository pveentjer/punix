#ifndef KERNEL_CONSOLE_H
#define KERNEL_CONSOLE_H

#include <stdint.h>
#include "sys/types.h"

enum console_esc_state
{
    CON_ESC_NONE = 0,
    CON_ESC_ESC,
    CON_ESC_CSI,
    CON_ESC_CSI_0,
    CON_ESC_CSI_2,
    CON_ESC_CSI_3,
    CON_ESC_CSI_OTHER
};

struct console;

/* Console driver operations */
struct console_ops
{
    void (*init)(struct console *con);

    void (*clear)(struct console *con);

    void (*put_char)(struct console *con, char c);

    void (*update_cursor)(struct console *con);

    void (*scroll)(struct console *con);
};

struct console
{
    const struct console_ops *ops;
    void *driver_data;  /* Driver-specific private data */

    int cols;
    int rows;
    int cursor_row;
    int cursor_col;

    uint8_t attr;
    enum console_esc_state esc_state;
};

/* Primary kernel console */
extern struct console kconsole;

void console_init();

void console_clear(struct console *con);

void console_put_char(struct console *con, char c);

int console_printf(struct console *con, const char *fmt, ...);

#define kprintf(...) console_printf(&kconsole, __VA_ARGS__)

/* Driver registration */
void console_register_vga(struct console *con);

#endif /* KERNEL_CONSOLE_H */