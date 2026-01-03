#include "kernel/tty.h"
#include "kernel/keyboard.h"
#include "kernel/console.h"
#include "kernel/sched.h"
#include "kernel/wait.h"
#include "kernel/kutils.h"
#include "kernel/dev.h"

#define TTY_INPUT_BUF_MASK  (TTY_INPUT_BUF_SIZE  - 1u)
#define TTY_OUTPUT_BUF_MASK (TTY_OUTPUT_BUF_SIZE - 1u)

/* Special markers for device registration */
#define TTY_MARKER_ACTIVE   ((void*)0)
#define TTY_MARKER_STDIN    ((void*)1)
#define TTY_MARKER_STDOUT   ((void*)2)
#define TTY_MARKER_STDERR   ((void*)3)

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

static void tty_init(struct tty *tty, size_t idx)
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
    tty->console = &kconsole;
    wait_queue_init(&tty->in_wait_queue);

}

static int tty_is_active(const struct tty *tty)
{
    return (tty != NULL) && (tty == ctx.active);
}

static bool tty_input_available(void *obj)
{
    struct tty *tty = (struct tty *)obj;
    return (tty->in_head - tty->in_tail) != 0;
}

/* ------------------------------------------------------------------
 * Device operations
 * ------------------------------------------------------------------ */

static int tty_open(struct file *file)
{
    if (file == NULL)
    {
        return -1;
    }

    uintptr_t marker = (uintptr_t)file->driver_data;

    /* Check if it's a special marker */
    if (marker <= 3)
    {
        struct task *curr = sched_current();

        if (marker >= 1 && marker <= 3)  /* stdin/stdout/stderr */
        {
            file->driver_data = (curr && curr->ctty) ? curr->ctty : tty_active();
        }
        else  /* TTY_MARKER_ACTIVE - /dev/tty */
        {
            file->driver_data = tty_active();
        }
    }
    /* else: already points to a specific tty (from /dev/ttyN registration) */

    if (file->driver_data == NULL)
    {
        return -1;
    }

    return 0;
}

static int tty_close(struct file *file)
{
    (void)file;
    return 0;
}

static ssize_t tty_read(struct file *file, void *buf, size_t count)
{
    if (file == NULL || buf == NULL || count == 0)
    {
        return -1;
    }

    struct tty *tty = (struct tty *)file->driver_data;
    if (tty == NULL)
    {
        return -1;
    }

    wait_event(&tty->in_wait_queue, tty_input_available, tty, WAIT_INTERRUPTIBLE);

    size_t available = tty->in_head - tty->in_tail;
    if (available > count)
    {
        available = count;
    }

    char *cbuf = (char *)buf;
    for (size_t i = 0; i < available; i++)
    {
        size_t idx = (tty->in_tail + i) & TTY_INPUT_BUF_MASK;
        cbuf[i] = tty->in_buf[idx];
    }

    tty->in_tail += available;

    return (ssize_t)available;
}

static ssize_t tty_write(struct file *file, const void *buf, size_t count)
{
    if (file == NULL || buf == NULL || count == 0)
    {
        return -1;
    }

    struct tty *tty = (struct tty *)file->driver_data;
    if (tty == NULL)
    {
        return -1;
    }

    const char *cbuf = (const char *)buf;
    size_t written = 0;

    while (written < count)
    {
        char c = cbuf[written];

        if (c == '\0')
        {
            break;
        }

        size_t used = tty->out_head - tty->out_tail;

        if (used >= TTY_OUTPUT_BUF_SIZE)
        {
            tty->out_tail++;
        }

        size_t idx = tty->out_head & TTY_OUTPUT_BUF_MASK;
        tty->out_buf[idx] = c;
        tty->out_head++;
        tty->cursor_pos = tty->out_head;

        written++;

        if (tty_is_active(tty) && (tty->console != NULL))
        {
            console_put_char(tty->console, c);
        }
    }

    return (ssize_t)written;
}

struct dev_ops tty_dev_ops = {
        .open = tty_open,
        .close = tty_close,
        .read = tty_read,
        .write = tty_write,
};

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
        return;
    }

    size_t idx = tty->in_head & TTY_INPUT_BUF_MASK;
    tty->in_buf[idx] = c;
    tty->in_head++;

    wakeup(&tty->in_wait_queue);
}

/* ------------------------------------------------------------------
 * Keyboard handler
 * ------------------------------------------------------------------ */

static void tty_keyboard_handler(char value, enum keyboard_code code, bool ctrl, bool alt, bool shift)
{
    (void)ctrl;
    (void)shift;

    struct tty *active = tty_active();

    if (value != 0)
    {
        tty_input_put(active, value);
        return;
    }

    if (alt)
    {
        if (code >= KEY_F1 && code <= KEY_F12)
        {
            size_t tty_idx = (size_t)(code - KEY_F1);

            if (tty_idx < TTY_COUNT)
            {
                tty_activate(tty_idx);
            }
            return;
        }
    }

    switch (code)
    {
        case KEY_UP:
            tty_input_put(active, 0x1b);
            tty_input_put(active, '[');
            tty_input_put(active, 'A');
            return;

        case KEY_DOWN:
            tty_input_put(active, 0x1b);
            tty_input_put(active, '[');
            tty_input_put(active, 'B');
            return;

        case KEY_RIGHT:
            tty_input_put(active, 0x1b);
            tty_input_put(active, '[');
            tty_input_put(active, 'C');
            return;

        case KEY_LEFT:
            tty_input_put(active, 0x1b);
            tty_input_put(active, '[');
            tty_input_put(active, 'D');
            return;

        default:
            break;
    }
}

/* ------------------------------------------------------------------
 * TTY system initialization
 * ------------------------------------------------------------------ */

void tty_system_init(void)
{
    for (size_t i = 0; i < TTY_COUNT; i++)
    {
        tty_init(&ctx.ttys[i], i);
    }

    ctx.active = &ctx.ttys[0];

    keyboard_init(tty_keyboard_handler);

    char dev_name[8];
    for (int i = 0; i < TTY_COUNT; i++)
    {
        k_snprintf(dev_name, sizeof(dev_name), "tty%d", i);
        dev_register(dev_name, &tty_dev_ops, &ctx.ttys[i]);
    }

    /* Register special devices with markers */
    dev_register("tty", &tty_dev_ops, TTY_MARKER_ACTIVE);
    dev_register("stdin", &tty_dev_ops, TTY_MARKER_STDIN);
    dev_register("stdout", &tty_dev_ops, TTY_MARKER_STDOUT);
    dev_register("stderr", &tty_dev_ops, TTY_MARKER_STDERR);
}

static void tty_redraw(struct tty *tty)
{
    if (tty == NULL || tty->console == NULL)
    {
        return;
    }

    console_clear(tty->console);

    size_t used = tty->out_head - tty->out_tail;

    for (size_t i = 0; i < used; i++)
    {
        size_t idx = (tty->out_tail + i) & TTY_OUTPUT_BUF_MASK;
        char c = tty->out_buf[idx];
        console_put_char(tty->console, c);
    }
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