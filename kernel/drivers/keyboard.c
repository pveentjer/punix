#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/interrupt.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------
 * I/O ports
 * ------------------------------------------------------------------ */
#define KEYBOARD_DATA_PORT 0x60
#define PIC1_COMMAND       0x20
#define PIC1_DATA          0x21

/* ------------------------------------------------------------------
 * Scancode constants (PS/2 Set 1)
 * ------------------------------------------------------------------ */

/* Special prefix */
#define SC_EXTENDED        0xE0

/* Shift keys */
#define SC_LSHIFT_PRESS    0x2A
#define SC_RSHIFT_PRESS    0x36
#define SC_LSHIFT_RELEASE  0xAA
#define SC_RSHIFT_RELEASE  0xB6

/* Ctrl keys */
#define SC_CTRL_PRESS      0x1D
#define SC_CTRL_RELEASE    0x9D

/* Alt keys */
#define SC_ALT_PRESS       0x38
#define SC_ALT_RELEASE     0xB8

/* Arrow keys (with 0xE0 prefix) */
#define SC_UP              0x48
#define SC_DOWN            0x50
#define SC_LEFT            0x4B
#define SC_RIGHT           0x4D

/* ------------------------------------------------------------------
 * Scancode tables
 * ------------------------------------------------------------------ */

static const char scancode_lower[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char scancode_upper[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

/* ------------------------------------------------------------------
 * State
 * ------------------------------------------------------------------ */
static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool extended_code = false;  /* 0xE0 prefix tracker */

/* ------------------------------------------------------------------
 * Port helpers
 * ------------------------------------------------------------------ */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ------------------------------------------------------------------
 * Keyboard ring buffer (monotonic head/tail)
 * ------------------------------------------------------------------ */

#define KBD_BUF_SIZE 128
#define KBD_BUF_MASK (KBD_BUF_SIZE - 1)

static char   kbd_buf[KBD_BUF_SIZE];
static size_t kbd_head = 0;  /* write position (monotonic) */
static size_t kbd_tail = 0;  /* read position (monotonic) */

/* Drop NEWEST when full (Linux-style) */
static void kbd_buffer_put(char c)
{
    size_t used = kbd_head - kbd_tail;

    if (used >= KBD_BUF_SIZE)
    {
        return; /* drop newest */
    }

    size_t idx = kbd_head & KBD_BUF_MASK;
    kbd_buf[idx] = c;
    kbd_head++;
}

/**
 * Read up to maxlen characters into buf.
 * Returns number of characters copied.
 * Non-blocking: returns 0 if no characters are available.
 */
size_t keyboard_read(char *buf, size_t maxlen)
{
    if (!buf || maxlen == 0)
    {
        return 0;
    }

    size_t available = kbd_head - kbd_tail;

    if (available == 0)
    {
        return 0;
    }

    if (available > maxlen)
    {
        available = maxlen;
    }

    for (size_t i = 0; i < available; i++)
    {
        size_t idx = (kbd_tail + i) & KBD_BUF_MASK;
        buf[i] = kbd_buf[idx];
    }

    kbd_tail += available;
    return available;
}

/* ------------------------------------------------------------------
 * Keyboard interrupt handler
 * ------------------------------------------------------------------ */

void keyboard_interrupt_handler(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Handle 0xE0 prefix (extended keys) */
    if (scancode == SC_EXTENDED)
    {
        extended_code = true;
        goto eoi;
    }

    if (extended_code)
    {
        extended_code = false;

        switch (scancode)
        {
            case SC_CTRL_PRESS:
            {
                ctrl_pressed = true;
                break;
            }
            case SC_CTRL_RELEASE:
            {
                ctrl_pressed = false;
                break;
            }
            case SC_ALT_PRESS:
            {
                alt_pressed = true;
                break;
            }
            case SC_ALT_RELEASE:
            {
                alt_pressed = false;
                break;
            }
            case SC_UP:
            {
                kbd_buffer_put('\x1b');
                kbd_buffer_put('[');
                kbd_buffer_put('A');
                break;
            }
            case SC_DOWN:
            {
                kbd_buffer_put('\x1b');
                kbd_buffer_put('[');
                kbd_buffer_put('B');
                break;
            }
            case SC_LEFT:
            {
                kbd_buffer_put('\x1b');
                kbd_buffer_put('[');
                kbd_buffer_put('D');
                break;
            }
            case SC_RIGHT:
            {
                kbd_buffer_put('\x1b');
                kbd_buffer_put('[');
                kbd_buffer_put('C');
                break;
            }
            default:
            {
                break; /* ignore others */
            }
        }

        goto eoi;
    }

    switch (scancode)
    {
        /* Shift press/release */
        case SC_LSHIFT_PRESS:
        case SC_RSHIFT_PRESS:
        {
            shift_pressed = true;
            break;
        }
        case SC_LSHIFT_RELEASE:
        case SC_RSHIFT_RELEASE:
        {
            shift_pressed = false;
            break;
        }

            /* Ctrl press/release */
        case SC_CTRL_PRESS:
        {
            ctrl_pressed = true;
            break;
        }
        case SC_CTRL_RELEASE:
        {
            ctrl_pressed = false;
            break;
        }

            /* Alt press/release */
        case SC_ALT_PRESS:
        {
            alt_pressed = true;
            break;
        }
        case SC_ALT_RELEASE:
        {
            alt_pressed = false;
            break;
        }

        default:
        {
            /* Printable range (only key press, not release) */
            if ((scancode & 0x80) == 0 && scancode < sizeof(scancode_lower))
            {
                char c = shift_pressed ? scancode_upper[scancode] : scancode_lower[scancode];

                if (c != 0)
                {
                    /* Ctrl mapping: Ctrl+[A–Z/a–z] → 0x01–0x1A */
                    if (ctrl_pressed)
                    {
                        if (c >= 'a' && c <= 'z')
                        {
                            c = (char)(c - 'a' + 1);
                        }
                        else if (c >= 'A' && c <= 'Z')
                        {
                            c = (char)(c - 'A' + 1);
                        }
                    }

                    /* Alt mapping: send ESC prefix + character */
                    if (alt_pressed)
                    {
                        kbd_buffer_put('\x1b');
                    }

                    kbd_buffer_put(c);
                }
            }
            break;
        }
    }

    eoi:
    outb(PIC1_COMMAND, 0x20); /* End of interrupt */
}

/* ------------------------------------------------------------------
 * Assembly ISR stub
 * ------------------------------------------------------------------ */

__attribute__((naked))
void keyboard_isr(void)
{
    asm volatile(
            "pushal\n\t"
            "call keyboard_interrupt_handler\n\t"
            "popal\n\t"
            "iret"
            );
}

/* ------------------------------------------------------------------
 * Keyboard initialization
 * ------------------------------------------------------------------ */

void keyboard_init(void)
{
    idt_set_gate(0x09, (uint32_t)keyboard_isr, 0x08, 0x8E);

    uint8_t mask = inb(PIC1_DATA);
    mask &= ~0x02;  /* Enable IRQ1 */
    outb(PIC1_DATA, mask);
}
