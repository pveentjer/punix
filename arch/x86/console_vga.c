#include <stdint.h>
#include "kernel/console.h"
#include "include/io.h"

#define VGA_TEXT_MODE_BUFFER    0xB8000
#define VGA_COLS                80
#define VGA_ROWS                25

/* VGA color codes */
#define VGA_BLACK       0
#define VGA_BLUE        1
#define VGA_GREEN       2
#define VGA_CYAN        3
#define VGA_RED         4
#define VGA_MAGENTA     5
#define VGA_BROWN       6
#define VGA_LIGHT_GRAY  7
#define VGA_DARK_GRAY   8
#define VGA_LIGHT_BLUE  9
#define VGA_LIGHT_GREEN 10
#define VGA_LIGHT_CYAN  11
#define VGA_LIGHT_RED   12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW      14
#define VGA_WHITE       15

#define VGA_ATTR(fg, bg) ((uint8_t)(((bg) << 4) | (fg)))

/* VGA-specific driver data */
struct vga_console_data
{
    volatile uint16_t *video;
    uint8_t ansi_fg;         // ANSI color (0-7)
    uint8_t ansi_bg;         // ANSI color (0-7)
    uint8_t bright;          // Bold flag
    int esc_params[8];
    int esc_param_count;
};

static inline uint16_t vga_entry(char c, uint8_t attr)
{
    return (uint16_t)c | ((uint16_t)attr << 8);
}

static uint8_t ansi_to_vga_color(int ansi, int bright)
{
    /* ANSI color mapping */
    switch (ansi)
    {
        case 0: return bright ? VGA_DARK_GRAY : VGA_BLACK;
        case 1: return bright ? VGA_LIGHT_RED : VGA_RED;
        case 2: return bright ? VGA_LIGHT_GREEN : VGA_GREEN;
        case 3: return bright ? VGA_YELLOW : VGA_BROWN;
        case 4: return bright ? VGA_LIGHT_BLUE : VGA_BLUE;
        case 5: return bright ? VGA_LIGHT_MAGENTA : VGA_MAGENTA;
        case 6: return bright ? VGA_LIGHT_CYAN : VGA_CYAN;
        case 7: return bright ? VGA_WHITE : VGA_LIGHT_GRAY;
        default: return VGA_LIGHT_GRAY;
    }
}

static void vga_update_attr(struct console *con)
{
    struct vga_console_data *vga = con->driver_data;
    uint8_t fg = ansi_to_vga_color(vga->ansi_fg, vga->bright);
    uint8_t bg = ansi_to_vga_color(vga->ansi_bg, 0);  // Background never bright
    con->attr = VGA_ATTR(fg, bg);
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

static void vga_handle_sgr(struct console *con)
{
    struct vga_console_data *vga = con->driver_data;

    if (vga->esc_param_count == 0)
    {
        /* ESC[m means reset */
        vga->ansi_fg = 7;  // White
        vga->ansi_bg = 0;  // Black
        vga->bright = 0;
        vga_update_attr(con);
        return;
    }

    for (int i = 0; i < vga->esc_param_count; i++)
    {
        int param = vga->esc_params[i];

        if (param == 0)
        {
            /* Reset */
            vga->ansi_fg = 7;
            vga->ansi_bg = 0;
            vga->bright = 0;
        }
        else if (param == 1)
        {
            /* Bright/bold */
            vga->bright = 1;
        }
        else if (param >= 30 && param <= 37)
        {
            /* Foreground color */
            vga->ansi_fg = param - 30;
        }
        else if (param >= 40 && param <= 47)
        {
            /* Background color */
            vga->ansi_bg = param - 40;
        }
    }

    vga_update_attr(con);
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
                vga->esc_param_count = 0;
                for (int i = 0; i < 8; i++)
                    vga->esc_params[i] = 0;
                return;
            }
            con->esc_state = CON_ESC_NONE;
            return;

        case CON_ESC_CSI:
            if (c >= '0' && c <= '9')
            {
                if (vga->esc_param_count == 0)
                    vga->esc_param_count = 1;

                int idx = vga->esc_param_count - 1;
                if (idx < 8)
                    vga->esc_params[idx] = vga->esc_params[idx] * 10 + (c - '0');
                return;
            }
            else if (c == ';')
            {
                if (vga->esc_param_count < 8)
                    vga->esc_param_count++;
                return;
            }
            else if (c == 'm')
            {
                /* SGR - Select Graphic Rendition (colors) */
                vga_handle_sgr(con);
                con->esc_state = CON_ESC_NONE;
                return;
            }
            else if (c == 'H')
            {
                con->cursor_row = con->cursor_col = 0;
                con->esc_state = CON_ESC_NONE;
                vga_update_cursor(con);
                return;
            }
            else if (c == 'J')
            {
                con->esc_state = CON_ESC_NONE;
                vga_clear(con);
                return;
            }
            /* Ignore other CSI sequences */
            if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E)
                con->esc_state = CON_ESC_NONE;
            return;

        default:
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
    vga->ansi_fg = 7;  // White
    vga->ansi_bg = 0;  // Black
    vga->bright = 0;
    vga->esc_param_count = 0;

    con->cols = VGA_COLS;
    con->rows = VGA_ROWS;
    vga_update_attr(con);
    con->cursor_row = 0;
    con->cursor_col = 0;
    con->esc_state = CON_ESC_NONE;

    /* Set VGA text mode 3 (80x25) */
    outb(0x3C2, 0x67);

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