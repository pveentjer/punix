#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/interrupt.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------
 * I/O ports / PIC (IRQ stuff – allowed to be a bit magic)
 * ------------------------------------------------------------------ */
#define KEYBOARD_DATA_PORT   0x60
#define PIC1_COMMAND_PORT    0x20
#define PIC1_DATA_PORT       0x21
#define PIC_EOI              0x20

#define KEYBOARD_IRQ_VECTOR  0x09
#define KEYBOARD_IRQ_MASK    (1u << 1)

/* ------------------------------------------------------------------
 * Scancode constants (PS/2 Set 1)
 * ------------------------------------------------------------------ */
#define SC_EXTENDED               0xE0
#define SCANCODE_RELEASE_MASK     0x80

/* Modifiers */
#define SC_LSHIFT                 0x2A
#define SC_RSHIFT                 0x36

#define SC_LCTRL                  0x1D

#define SC_LALT                   0x38

/* Extended (0xE0-prefixed) modifiers */
#define SC_RCTRL                  0x1D
#define SC_RALT                   0x38

/* Arrow keys (extended) */
#define SC_ARROW_UP               0x48
#define SC_ARROW_DOWN             0x50
#define SC_ARROW_LEFT             0x4B
#define SC_ARROW_RIGHT            0x4D

/* Function keys (non-extended) */
#define SC_F1                     0x3B
#define SC_F2                     0x3C
#define SC_F3                     0x3D
#define SC_F4                     0x3E
#define SC_F5                     0x3F
#define SC_F6                     0x40
#define SC_F7                     0x41
#define SC_F8                     0x42
#define SC_F9                     0x43
#define SC_F10                    0x44
#define SC_F11                    0x57
#define SC_F12                    0x58

/* ------------------------------------------------------------------
 * Scancode → ASCII tables
 * ------------------------------------------------------------------ */
static const char SCANCODE_LOWER[] = {
        0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
        'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0, 'a', 's',
        'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\', 'z', 'x', 'c', 'v',
        'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static const char SCANCODE_UPPER[] = {
        0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b', '\t',
        'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0, 'A', 'S',
        'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~', 0, '|', 'Z', 'X', 'C', 'V',
        'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' '
};

/* ------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------ */
static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool extended_code = false;

/* User-supplied handler */
static void (*keyboard_handler_cb)(char value, enum keyboard_code code) = NULL;

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
 * Helper to emit a key event if handler is set
 * ------------------------------------------------------------------ */
static void emit_key(char value, enum keyboard_code code)
{
    if (keyboard_handler_cb != NULL)
    {
        keyboard_handler_cb(value, code);
    }
}

/* ------------------------------------------------------------------
 * Main IRQ-level handler
 * ------------------------------------------------------------------ */
void keyboard_interrupt_handler(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    /* Handle 0xE0 prefix for extended keys */
    if (scancode == SC_EXTENDED)
    {
        extended_code = true;
        goto eoi;
    }

    if (extended_code)
    {
        extended_code = false;

        bool is_release = ((scancode & SCANCODE_RELEASE_MASK) != 0);
        uint8_t base = (uint8_t)(scancode & (uint8_t)~SCANCODE_RELEASE_MASK);

        switch (base)
        {
            /* Extended modifiers (right Ctrl/Alt) */
            case SC_RCTRL:
            {
                if (is_release)
                {
                    ctrl_pressed = false;
                }
                else
                {
                    ctrl_pressed = true;
                }
                break;
            }
            case SC_RALT:
            {
                if (is_release)
                {
                    alt_pressed = false;
                }
                else
                {
                    alt_pressed = true;
                }
                break;
            }

                /* Arrows */
            case SC_ARROW_UP:
            {
                if (!is_release)
                {
                    emit_key(0, KEY_UP);
                }
                break;
            }
            case SC_ARROW_DOWN:
            {
                if (!is_release)
                {
                    emit_key(0, KEY_DOWN);
                }
                break;
            }
            case SC_ARROW_LEFT:
            {
                if (!is_release)
                {
                    emit_key(0, KEY_LEFT);
                }
                break;
            }
            case SC_ARROW_RIGHT:
            {
                if (!is_release)
                {
                    emit_key(0, KEY_RIGHT);
                }
                break;
            }

            default:
            {
                break;
            }
        }

        goto eoi;
    }

    /* Non-extended scancode */
    {
        bool is_release = ((scancode & SCANCODE_RELEASE_MASK) != 0);
        uint8_t base = (uint8_t)(scancode & (uint8_t)~SCANCODE_RELEASE_MASK);

        switch (base)
        {
            /* ---------------- Modifiers ---------------- */
            case SC_LSHIFT:
            case SC_RSHIFT:
            {
                if (is_release)
                {
                    shift_pressed = false;
                }
                else
                {
                    shift_pressed = true;
                }
                goto eoi;
            }

            case SC_LCTRL:
            {
                if (is_release)
                {
                    ctrl_pressed = false;
                }
                else
                {
                    ctrl_pressed = true;
                }
                goto eoi;
            }

            case SC_LALT:
            {
                if (is_release)
                {
                    alt_pressed = false;
                }
                else
                {
                    alt_pressed = true;
                }
                goto eoi;
            }

                /* ---------------- Function keys ---------------- */
            case SC_F1:
            case SC_F2:
            case SC_F3:
            case SC_F4:
            case SC_F5:
            case SC_F6:
            case SC_F7:
            case SC_F8:
            case SC_F9:
            case SC_F10:
            case SC_F11:
            case SC_F12:
            {
                if (!is_release)
                {
                    enum keyboard_code code = KEY_NONE;

                    switch (base)
                    {
                        case SC_F1:  { code = KEY_F1;  break; }
                        case SC_F2:  { code = KEY_F2;  break; }
                        case SC_F3:  { code = KEY_F3;  break; }
                        case SC_F4:  { code = KEY_F4;  break; }
                        case SC_F5:  { code = KEY_F5;  break; }
                        case SC_F6:  { code = KEY_F6;  break; }
                        case SC_F7:  { code = KEY_F7;  break; }
                        case SC_F8:  { code = KEY_F8;  break; }
                        case SC_F9:  { code = KEY_F9;  break; }
                        case SC_F10: { code = KEY_F10; break; }
                        case SC_F11: { code = KEY_F11; break; }
                        case SC_F12: { code = KEY_F12; break; }
                        default:     { code = KEY_NONE; break; }
                    }

                    if (code != KEY_NONE)
                    {
                        emit_key(0, code);
                    }
                }

                goto eoi;
            }

            default:
            {
                /* fall through to printable handling below */
                break;
            }
        }

        /* ---------------- Printable keys ---------------- */
        if (is_release)
        {
            goto eoi;
        }

        if (base >= sizeof(SCANCODE_LOWER))
        {
            goto eoi;
        }

        char c;

        if (shift_pressed)
        {
            c = SCANCODE_UPPER[base];
        }
        else
        {
            c = SCANCODE_LOWER[base];
        }

        if (c == 0)
        {
            goto eoi;
        }

        /* Ctrl-modified letters → control codes (1..26) */
        if (ctrl_pressed)
        {
            if ((c >= 'a') && (c <= 'z'))
            {
                c = (char)(c - 'a' + 1);
            }
            else if ((c >= 'A') && (c <= 'Z'))
            {
                c = (char)(c - 'A' + 1);
            }
        }

        emit_key(c, KEY_NONE);
    }

    eoi:
    outb(PIC1_COMMAND_PORT, PIC_EOI);
}

/* ------------------------------------------------------------------
 * ISR stub (assembly wrapper)
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
 * Initialization
 * ------------------------------------------------------------------ */
void keyboard_init(void (*handler)(char value, enum keyboard_code code))
{
    keyboard_handler_cb = handler;

    shift_pressed = false;
    ctrl_pressed  = false;
    alt_pressed   = false;
    extended_code = false;

    idt_set_gate(KEYBOARD_IRQ_VECTOR, (uint32_t)keyboard_isr, 0x08, 0x8E);

    uint8_t mask = inb(PIC1_DATA_PORT);
    mask &= (uint8_t)~KEYBOARD_IRQ_MASK;   /* enable IRQ1 */
    outb(PIC1_DATA_PORT, mask);
}
