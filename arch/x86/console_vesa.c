#include <stdint.h>
#include <stddef.h>
#include "kernel/console.h"
#include "kernel/kutils.h"

/*
 * IMPORTANT:
 * - This driver requires a *mapped, dereferenceable* framebuffer pointer.
 * - The bootloader (or early kernel VBE code) must:
 *     1) set VBE mode
 *     2) obtain PhysBasePtr + pitch + width/height/bpp
 *     3) map the framebuffer physical range into kernel VA space
 *     4) pass the resulting VA here via console_register_vesa()
 *
 * This file does NOT guess framebuffer addresses.
 */

#define VESA_DEFAULT_BPP     32

#define VGA_FONT_ROM_8x16    0xC0000  /* VGA BIOS ROM base (requires mapping if paging enabled) */
#define FONT_HEIGHT          16
#define FONT_WIDTH           8

/* VESA-specific driver data */
struct vesa_console_data
{
    uint32_t *framebuffer;     /* MUST be a mapped VA */
    uint32_t width;
    uint32_t height;
    uint32_t pitch;            /* bytes per scanline */
    uint32_t bpp;

    const uint8_t *font;       /* pointer to font glyphs (requires mapping if paging enabled) */

    uint32_t fg_color;
    uint32_t bg_color;
};

static struct vesa_console_data vesa_data;

/* Read font from VGA BIOS ROM */
static void vesa_load_font(struct vesa_console_data *vesa)
{
    /* Font data is at offset 0x4000 in VGA BIOS ROM for 8x16 font */
    vesa->font = (const uint8_t *)(uintptr_t)(VGA_FONT_ROM_8x16 + 0x4000);
}

static void vesa_draw_glyph(struct vesa_console_data *vesa, int x, int y, char c)
{
    const uint8_t *glyph = &vesa->font[(unsigned char)c * FONT_HEIGHT];
    uint32_t *fb = vesa->framebuffer + (y * FONT_HEIGHT * (vesa->pitch / 4)) + x * FONT_WIDTH;

    for (int row = 0; row < FONT_HEIGHT; row++)
    {
        uint8_t bits = glyph[row];
        uint32_t *pixel = fb + row * (vesa->pitch / 4);

        for (int col = 0; col < FONT_WIDTH; col++)
        {
            pixel[col] = (bits & 0x80) ? vesa->fg_color : vesa->bg_color;
            bits <<= 1;
        }
    }
}

static void vesa_scroll(struct console *con)
{
    struct vesa_console_data *vesa = con->driver_data;
    uint32_t *fb = vesa->framebuffer;

    uint32_t line_bytes  = FONT_HEIGHT * vesa->pitch;
    uint32_t total_bytes = (con->rows - 1) * line_bytes;

    k_memmove(fb, fb + (line_bytes / 4), total_bytes);

    uint32_t *last_line = fb + ((con->rows - 1) * line_bytes / 4);
    for (uint32_t i = 0; i < line_bytes / 4; i++)
        last_line[i] = vesa->bg_color;

    con->cursor_row = con->rows - 1;
    con->cursor_col = 0;
}

static void vesa_clear(struct console *con)
{
    struct vesa_console_data *vesa = con->driver_data;
    uint32_t *fb = vesa->framebuffer;
    uint32_t total_pixels = (vesa->height * (vesa->pitch / 4));

    for (uint32_t i = 0; i < total_pixels; i++)
        fb[i] = vesa->bg_color;

    con->cursor_row = 0;
    con->cursor_col = 0;
    con->esc_state = CON_ESC_NONE;
}

static void vesa_update_cursor(struct console *con)
{
    struct vesa_console_data *vesa = con->driver_data;

    int x = con->cursor_col;
    int y = con->cursor_row;

    uint32_t *fb = vesa->framebuffer + (y * FONT_HEIGHT * (vesa->pitch / 4)) + x * FONT_WIDTH;

    for (int row = FONT_HEIGHT - 2; row < FONT_HEIGHT; row++)
    {
        uint32_t *pixel = fb + row * (vesa->pitch / 4);
        for (int col = 0; col < FONT_WIDTH; col++)
            pixel[col] = vesa->fg_color;
    }
}

static void vesa_put_char(struct console *con, char c)
{
    struct vesa_console_data *vesa = con->driver_data;

    switch (con->esc_state)
    {
        case CON_ESC_NONE:
            if ((unsigned char)c == 0x1B) { con->esc_state = CON_ESC_ESC; return; }
            break;

        case CON_ESC_ESC:
            if (c == '[') { con->esc_state = CON_ESC_CSI; return; }
            con->esc_state = CON_ESC_NONE;
            return;

        case CON_ESC_CSI:
            if (c == 'H') { con->cursor_row = con->cursor_col = 0; con->esc_state = CON_ESC_NONE; return; }
            if (c == 'J') { con->esc_state = CON_ESC_NONE; vesa_clear(con); return; }
            if (c == '0') { con->esc_state = CON_ESC_CSI_0; return; }
            if (c == '2') { con->esc_state = CON_ESC_CSI_2; return; }
            if (c == '3') { con->esc_state = CON_ESC_CSI_3; return; }
            con->esc_state = CON_ESC_CSI_OTHER;
            return;

        case CON_ESC_CSI_0:
        case CON_ESC_CSI_2:
        case CON_ESC_CSI_3:
            if (c == 'H') { con->cursor_row = con->cursor_col = 0; con->esc_state = CON_ESC_NONE; return; }
            if (c == 'J') { con->esc_state = CON_ESC_NONE; vesa_clear(con); return; }
            con->esc_state = CON_ESC_NONE;
            return;

        case CON_ESC_CSI_OTHER:
        default:
            if ((unsigned char)c >= 0x40 && (unsigned char)c <= 0x7E)
                con->esc_state = CON_ESC_NONE;
            return;
    }

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
        {
            con->cursor_col--;
            vesa_draw_glyph(vesa, con->cursor_col, con->cursor_row, ' ');
        }
        else if (con->cursor_row > 0)
        {
            con->cursor_row--;
            con->cursor_col = con->cols - 1;
            vesa_draw_glyph(vesa, con->cursor_col, con->cursor_row, ' ');
        }
        return;
    }
    else if (c == '\t')
    {
        int tabw = 8;
        int next = (con->cursor_col + tabw) & ~(tabw - 1);
        if (next >= con->cols) next = con->cols - 1;

        while (con->cursor_col < next)
        {
            vesa_draw_glyph(vesa, con->cursor_col, con->cursor_row, ' ');
            con->cursor_col++;
        }
    }
    else
    {
        vesa_draw_glyph(vesa, con->cursor_col, con->cursor_row, c);
        con->cursor_col++;
        if (con->cursor_col >= con->cols)
        {
            con->cursor_col = 0;
            con->cursor_row++;
        }
    }

    if (con->cursor_row >= con->rows)
        vesa_scroll(con);
}

static void vesa_init(struct console *con)
{
    struct vesa_console_data *vesa = con->driver_data;

    /* Must be provided by console_register_vesa() */
    if (vesa->framebuffer == NULL || vesa->width == 0 || vesa->height == 0 || vesa->pitch == 0)
        panic("vesa: framebuffer/geometry not provided (need mapped VA + width/height/pitch)");

    if (vesa->bpp == 0)
        vesa->bpp = VESA_DEFAULT_BPP;

    /* This driver assumes 32bpp XRGB/RGBX */
    if (vesa->bpp != 32)
        panic("vesa: only 32bpp supported");

    con->cols = vesa->width / FONT_WIDTH;
    con->rows = vesa->height / FONT_HEIGHT;
    con->attr = 0x07;
    con->cursor_row = 0;
    con->cursor_col = 0;
    con->esc_state = CON_ESC_NONE;

    vesa->fg_color = 0x00FFFFFF;
    vesa->bg_color = 0x00000000;

    vesa_load_font(vesa);
    vesa_clear(con);
}

static const struct console_ops vesa_console_ops = {
        .init          = vesa_init,
        .clear         = vesa_clear,
        .put_char      = vesa_put_char,
        .update_cursor = vesa_update_cursor,
        .scroll        = vesa_scroll
};

void console_register_vesa(struct console *con,
                           void *framebuffer_va,
                           uint32_t width,
                           uint32_t height,
                           uint32_t pitch,
                           uint32_t bpp)
{
    if (framebuffer_va == NULL || width == 0 || height == 0 || pitch == 0)
        panic("console_register_vesa: invalid framebuffer/geometry");

    vesa_data.framebuffer = (uint32_t *)framebuffer_va; /* mapped VA */
    vesa_data.width = width;
    vesa_data.height = height;
    vesa_data.pitch = pitch;
    vesa_data.bpp = bpp;

    con->ops = &vesa_console_ops;
    con->driver_data = &vesa_data;
}
