#include "../../include/kernel/tty.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/console.h"

#define TTY_INPUT_BUF_MASK  (TTY_INPUT_BUF_SIZE  - 1u)
#define TTY_OUTPUT_BUF_MASK (TTY_OUTPUT_BUF_SIZE - 1u)

/* ------------------------------------------------------------------
 * Internal TTY context
 * ------------------------------------------------------------------ */

struct tty_context
{
    struct tty ttys[TTY_COUNT];
    size_t     active_idx;
    struct tty *active;
};

static struct tty_context ctx;

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

static void tty_init(struct tty *tty, size_t idx)
{
    if (!tty)
    {
        return;
    }

    tty->idx        = idx;
    tty->in_head    = 0u;
    tty->in_tail    = 0u;
    tty->out_head   = 0u;
    tty->out_tail   = 0u;
    tty->cursor_pos = 0u;
    tty->console    = &kconsole; /* all TTYs share the same physical console */
}

static int tty_is_active(const struct tty *tty)
{
    return (tty != NULL) && (tty == ctx.active);
}

/* ------------------------------------------------------------------
 * TTY input
 * ------------------------------------------------------------------ */

void tty_input_put(struct tty *tty, char c)
{
    if (tty == NULL)
    {
        return;
    }

    size_t used = tty->in_head - tty->in_tail;

    if (used >= TTY_INPUT_BUF_SIZE)
    {
        /* Buffer full: drop newest character. */
        return;
    }

    size_t idx = tty->in_head & TTY_INPUT_BUF_MASK;
    tty->in_buf[idx] = c;
    tty->in_head++;
}

size_t tty_read(struct tty *tty, char *buf, size_t maxlen)
{
    if ((tty == NULL) || (buf == NULL) || (maxlen == 0u))
    {
        return 0u;
    }

    size_t available = tty->in_head - tty->in_tail;

    if (available == 0u)
    {
        return 0u;
    }

    if (available > maxlen)
    {
        available = maxlen;
    }

    for (size_t i = 0u; i < available; i++)
    {
        size_t idx = (tty->in_tail + i) & TTY_INPUT_BUF_MASK;
        buf[i] = tty->in_buf[idx];
    }

    tty->in_tail += available;
    return available;
}

/* ------------------------------------------------------------------
 * TTY output
 * ------------------------------------------------------------------ */

size_t tty_write(struct tty *tty, const char *buf, size_t maxlen)
{
    if ((tty == NULL) || (buf == NULL) || (maxlen == 0u))
    {
        return 0u;
    }

    size_t written = 0u;

    while (written < maxlen)
    {
        char c = buf[written];

        if (c == '\0')
        {
            break;
        }

        size_t used = tty->out_head - tty->out_tail;

        if (used >= TTY_OUTPUT_BUF_SIZE)
        {
            /* Buffer full: drop oldest character. */
            tty->out_tail++;
        }

        size_t idx = tty->out_head & TTY_OUTPUT_BUF_MASK;
        tty->out_buf[idx] = c;
        tty->out_head++;
        tty->cursor_pos = tty->out_head; /* simple logical cursor */

        written++;

        /* Forward to console only if this is the ACTIVE TTY. */
        if (tty_is_active(tty) && (tty->console != NULL))
        {

            console_put_char(tty->console, c);
        }
    }

    return written;
}

/* ------------------------------------------------------------------
 * Keyboard handler
 * ------------------------------------------------------------------ */

static void tty_keyboard_handler(char value, enum keyboard_code code)
{
    struct tty *active = tty_active();

    if (value != 0)
    {
        /* Printable character: push into active TTY input. */
        tty_input_put(active, value);
        return;
    }

    /* Non-printable / special key: handle function keys for TTY switching. */
    switch (code)
    {
        case KEY_F1:
        {
            kprintf("Ctrl+Alt+F1 pressed\n");
            tty_activate(0u);
            break;
        }
        case KEY_F2:
        {
            kprintf("Ctrl+Alt+F2 pressed\n");
            tty_activate(1u);
            break;
        }
        case KEY_F3:
        {
            kprintf("Ctrl+Alt+F3 pressed\n");
            tty_activate(2u);
            break;
        }
        case KEY_F4:
        {
            kprintf("Ctrl+Alt+F4 pressed\n");
            tty_activate(3u);
            break;
        }
        case KEY_F5:
        {
            kprintf("Ctrl+Alt+F5 pressed\n");
            tty_activate(4u);
            break;
        }
        case KEY_F6:
        {
            kprintf("Ctrl+Alt+F6 pressed\n");
            tty_activate(5u);
            break;
        }
        case KEY_F7:
        {
            kprintf("Ctrl+Alt+F7 pressed\n");
            tty_activate(6u);
            break;
        }
        case KEY_F8:
        {
            kprintf("Ctrl+Alt+F8 pressed\n");
            tty_activate(7u);
            break;
        }
        case KEY_F9:
        {
            kprintf("Ctrl+Alt+F9 pressed\n");
            tty_activate(8u);
            break;
        }
        case KEY_F10:
        {
            kprintf("Ctrl+Alt+F10 pressed\n");
            tty_activate(9u);
            break;
        }
        case KEY_F11:
        {
            kprintf("Ctrl+Alt+F11 pressed\n");
            tty_activate(10u);
            break;
        }
        case KEY_F12:
        {
            kprintf("Ctrl+Alt+F12 pressed\n");
            tty_activate(11u);
            break;
        }
        default:
        {
            /* ignore for now */
            break;
        }
    }
}


void tty_system_init(void)
{
    for (size_t i = 0u; i < TTY_COUNT; i++)
    {
        tty_init(&ctx.ttys[i], i);
    }

    ctx.active_idx = 0u;
    ctx.active     = &ctx.ttys[0];

    keyboard_init(tty_keyboard_handler);
}

void tty_activate(size_t idx)
{
    if (idx >= TTY_COUNT)
    {
        return;
    }

    ctx.active_idx = idx;
    ctx.active     = &ctx.ttys[idx];

    if (ctx.active->console != NULL)
    {
        /* For now, just clear the console when switching.
         * Later you can redraw from ctx.active->out_buf.
         */
        console_clear(ctx.active->console);
    }
}

struct tty *tty_active(void)
{
    return ctx.active;
}

struct tty *tty_get(size_t idx)
{
    if (idx >= TTY_COUNT)
    {
        return NULL;
    }
    return &ctx.ttys[idx];
}
