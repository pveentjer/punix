#include <stdint.h>
#include "kernel/console.h"

#define VGA_TEXT_MODE_BUFFER    0xB8000
#define VGA_COLS                80
#define VGA_ROWS                25
#define VGA_ATTR_WHITE_ON_BLACK 0x07

/* VGA-specific driver data */
struct vga_console_data
{
    volatile uint16_t *video;
};

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" :: "a"(value), "Nd"(port));
}

static void vga_update_cursor(struct console *con)
{
    uint16_t pos = con->cursor_row * con->cols + con->cursor_col;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

static void vga_scroll(struct console *con)
{
    struct vga_console_data *vga = con->driver_data;
    volatile uint16_t *video = vga->video;

    /* Move rows up */
    for (int row = 0; row < con->rows - 1; row++)
        for (int col = 0; col < con->cols; col++)
            video[row * con->cols + col] = video[(row + 1) * con->cols + col];

    /* Clear last row */
    for (int col = 0; col < con->cols; col++)
        video[(con->rows - 1) * con->cols + col] = vga_entry(' ', con->attr);

    con->cursor_row = con->rows - 1;
    con->cursor_col = 0;
}

static void vga_clear(struct console *con)
{
    struct vga_console_data *vga = con->driver_data;
    volatile uint16_t *video = vga->video;
    int total = con->cols * con->rows;

    for (int i = 0; i < total; i++)
        video[i] = vga_entry(' ', con->attr);

    con->cursor_row = 0;
    con->cursor_col = 0;
    con->esc_state = CON_ESC_NONE;

    vga_update_cursor(con);
}

static void vga_put_char(struct console *con, char c)
{
    struct vga_console_data *vga = con->driver_data;
    volatile uint16_t *video = vga->video;

    /* Handle ANSI escape sequences */
    switch (con->esc_state)
    {
        case CON_ESC_NONE:
            if ((unsigned char)c == 0x1B)
            {
                con->esc_state = CON_ESC_ESC;
                return;
            }
            break;

        case CON_ESC_ESC:
            if (c == '[')
            {
                con->esc_state = CON_ESC_CSI;
                return;
            }
            con->esc_state = CON_ESC_NONE;
            return;

        case CON_ESC_CSI:
            if (c == 'H')
            {
                con->cursor_row = con->cursor_col = 0;
                con->esc_state = CON_ESC_NONE;
                vga_update_cursor(con);
                return;
            }
            if (c == 'J')
            {
                con->esc_state = CON_ESC_NONE;
                vga_clear(con);
                return;
            }
            if (c == '0') { con->esc_state = CON_ESC_CSI_0; return; }
            if (c == '2') { con->esc_state = CON_ESC_CSI_2; return; }
            if (c == '3') { con->esc_state = CON_ESC_CSI_3; return; }
            con->esc_state = CON_ESC_CSI_OTHER;
            return;

        case CON_ESC_CSI_0:
        case CON_ESC_CSI_2:
        case CON_ESC_CSI_3:
            if (c == 'H')
            {
                con->cursor_row = con->cursor_col = 0;
                con->esc_state = CON_ESC_NONE;
                vga_update_cursor(con);
                return;
            }
            if (c == 'J')
            {
                con->esc_state = CON_ESC_NONE;
                vga_clear(con);
                return;
            }
            con->esc_state = CON_ESC_NONE;
            return;

        case CON_ESC_CSI_OTHER:
        default:
            if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E)
                con->esc_state = CON_ESC_NONE;
            return;
    }

    /* Normal character output */
    if (c == '\n')
    {
        con->cursor_col = 0;
        con->cursor_row++;
    }
    else if (c == '\r')
    {
        con->cursor_col = 0;
    }
    else if (c == '\b')
    {
        if (con->cursor_col > 0)
            con->cursor_col--;
        else if (con->cursor_row > 0)
        {
            con->cursor_row--;
            con->cursor_col = con->cols - 1;
        }
        else
        {
            vga_update_cursor(con);
            return;
        }
        video[con->cursor_row * con->cols + con->cursor_col] = vga_entry(' ', con->attr);
    }
    else if (c == '\t')
    {
        int tabw = 8;
        int next = (con->cursor_col + tabw) & ~(tabw - 1);
        if (next >= con->cols) next = con->cols - 1;

        while (con->cursor_col < next)
        {
            video[con->cursor_row * con->cols + con->cursor_col] = vga_entry(' ', con->attr);
            con->cursor_col++;
        }
    }
    else
    {
        video[con->cursor_row * con->cols + con->cursor_col] = vga_entry(c, con->attr);
        con->cursor_col++;
        if (con->cursor_col >= con->cols)
        {
            con->cursor_col = 0;
            con->cursor_row++;
        }
    }

    if (con->cursor_row >= con->rows)
        vga_scroll(con);

    vga_update_cursor(con);
}

static void vga_init(struct console *con)
{
    struct vga_console_data *vga = con->driver_data;
    vga->video = (volatile uint16_t *)VGA_TEXT_MODE_BUFFER;

    con->cols = VGA_COLS;
    con->rows = VGA_ROWS;
    con->attr = VGA_ATTR_WHITE_ON_BLACK;
    con->cursor_row = 0;
    con->cursor_col = 0;
    con->esc_state = CON_ESC_NONE;

    vga_clear(con);
}

static const struct console_ops vga_console_ops = {
        .init = vga_init,
        .clear = vga_clear,
        .put_char = vga_put_char,
        .update_cursor = vga_update_cursor,
        .scroll = vga_scroll
};

static struct vga_console_data vga_data;

void console_register_vga(struct console *con)
{
    con->ops = &vga_console_ops;
    con->driver_data = &vga_data;
}