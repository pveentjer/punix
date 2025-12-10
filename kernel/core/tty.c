#include "../../include/kernel/tty.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/vga.h"

#define TTY_INPUT_BUF_MASK (TTY_INPUT_BUF_SIZE - 1u)


struct tty tty0;

void tty_init(struct tty *tty)
{
    if (tty == NULL)
    {
        return;
    }

    tty->in_head = 0u;
    tty->in_tail = 0u;
}

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

static void tty_keyboard_handler(char value, enum keyboard_code code)
{
    if (value != 0)
    {
        tty_input_put(&tty0, value);
        return;
    }

    switch (code)
    {
        case KEY_F1:
        {
            kprintf("Ctrl+Alt+F1 pressed\n");
            break;
        }
        case KEY_F2:
        {
            kprintf("Ctrl+Alt+F2 pressed\n");
            break;
        }
        case KEY_F3:
        {
            kprintf("Ctrl+Alt+F3 pressed\n");
            break;
        }
        case KEY_F4:
        {
            kprintf("Ctrl+Alt+F4 pressed\n");
            break;
        }
        case KEY_F5:
        {
            kprintf("Ctrl+Alt+F5 pressed\n");
            break;
        }
        case KEY_F6:
        {
            kprintf("Ctrl+Alt+F6 pressed\n");
            break;
        }
        case KEY_F7:
        {
            kprintf("Ctrl+Alt+F7 pressed\n");
            break;
        }
        case KEY_F8:
        {
            kprintf("Ctrl+Alt+F8 pressed\n");
            break;
        }
        case KEY_F9:
        {
            kprintf("Ctrl+Alt+F9 pressed\n");
            break;
        }
        case KEY_F10:
        {
            kprintf("Ctrl+Alt+F10 pressed\n");
            break;
        }
        case KEY_F11:
        {
            kprintf("Ctrl+Alt+F11 pressed\n");
            break;
        }
        case KEY_F12:
        {
            kprintf("Ctrl+Alt+F12 pressed\n");
            break;
        }
        default:
        {
            /* ignore for now */
            break;
        }
    }
}

/* ------------------------------------------------------------------
 * TTY subsystem initialization
 * ------------------------------------------------------------------ */

void tty_system_init(void)
{
    /* Initialize the primary TTY. */
    tty_init(&tty0);

    /* Configure keyboard to call our handler. */
    keyboard_init(tty_keyboard_handler);
}
