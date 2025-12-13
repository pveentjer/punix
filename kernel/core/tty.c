#include "../../include/kernel/tty.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/console.h"
#include "kernel/sched.h"

#define TTY_INPUT_BUF_MASK  (TTY_INPUT_BUF_SIZE  - 1u)
#define TTY_OUTPUT_BUF_MASK (TTY_OUTPUT_BUF_SIZE - 1u)

/* ------------------------------------------------------------------
 * Internal TTY context
 * ------------------------------------------------------------------ */

struct tty_context
{
    struct tty ttys[TTY_COUNT];
    struct tty *active;
};

static struct tty_context ctx;

/* ------------------------------------------------------------------
 * Internal helpers
 * ------------------------------------------------------------------ */

static void tty_init(
        struct tty *tty,
        size_t idx)
{
    if (!tty)
    {
        return;
    }

    tty->idx = idx;
    tty->in_head = 0u;
    tty->in_tail = 0u;
    tty->out_head = 0u;
    tty->out_tail = 0u;
    tty->cursor_pos = 0u;
    tty->console = &kconsole; /* all TTYs share the same physical console */
    wait_queue_init(&tty->in_wait_queue);
}

static int tty_is_active(
        const struct tty *tty)
{
    return (tty != NULL) && (tty == ctx.active);
}

/* ------------------------------------------------------------------
 * TTY input
 * ------------------------------------------------------------------ */

void tty_input_put(
        struct tty *tty, char c)
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

    wakeup(&tty->in_wait_queue);
}

size_t tty_read(
        struct tty *tty,
        char *buf,
        size_t maxlen)
{
    if ((tty == NULL) || (buf == NULL) || (maxlen == 0u))
    {
        return 0u;
    }

    size_t available = tty->in_head - tty->in_tail;

    while (available == 0)
    {
        struct task *task = sched_current();
        task->state = TASK_BLOCKED;

        struct wait_queue_entry entry = {
                .task = task,
                .next = NULL,
        };

        wait_queue_add(&tty->in_wait_queue, &entry);

        sched_schedule();

        available = tty->in_head - tty->in_tail;
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

}

/* ------------------------------------------------------------------
 * TTY output
 * ------------------------------------------------------------------ */

size_t tty_write(
        struct tty *tty,
        const char *buf,
        size_t maxlen)
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

static void tty_keyboard_handler(
        char value,
        enum keyboard_code code,
        bool ctrl,
        bool alt,
        bool shift)
{
    struct tty *active = tty_active();

    if (value != 0)
    {
        /* Printable character: push into active TTY input. */
        tty_input_put(active, value);
        return;
    }

    if (alt)
    {
        // no check for ctrl because when running virtualized, the host should not
        // be triggered to switch tty. So when running virtualized, use alt-fn
        if (code >= KEY_F1 && code <= KEY_F12)
        {
            size_t tty_idx = (size_t) (code - KEY_F1);

            if (tty_idx < TTY_COUNT)
            {
                tty_activate(tty_idx);
            }
            return;
        }
    }
}

void tty_system_init(void)
{
    for (size_t i = 0u; i < TTY_COUNT; i++)
    {
        tty_init(&ctx.ttys[i], i);
    }

    ctx.active = &ctx.ttys[0];

    keyboard_init(tty_keyboard_handler);
}

static void tty_redraw(struct tty *tty)
{
    if (tty == NULL || tty->console == NULL)
    {
        return;
    }

    console_clear(tty->console);

    /* Re-play all characters currently stored in the output ring. */
    size_t used = tty->out_head - tty->out_tail;

    for (size_t i = 0; i < used; i++)
    {
        size_t idx = (tty->out_tail + i) & TTY_OUTPUT_BUF_MASK;
        char c = tty->out_buf[idx];

        /* This updates the *physical* cursor and scrolls as needed. */
        console_put_char(tty->console, c);
    }

    /* Logical cursor position is already tty->out_head; if you want to
     * explicitly sync, you can keep this, but it's redundant:
     *
     * tty->cursor_pos = tty->out_head;
     */
}

void tty_activate(size_t idx)
{
    if (idx >= TTY_COUNT)
    {
        return;
    }

    ctx.active = &ctx.ttys[idx];

    tty_redraw(ctx.active);
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
