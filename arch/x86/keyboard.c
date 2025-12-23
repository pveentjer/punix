#include "kernel/keyboard.h"
#include "kernel/irq.h"
#include "include/irq.h"

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------------
 * I/O ports / PIC
 * ------------------------------------------------------------------ */
#define KEYBOARD_DATA_PORT   0x60
#define PIC1_DATA_PORT       0x21

#define KEYBOARD_IRQ_VECTOR  0x09
#define KEYBOARD_IRQ_MASK    (1u << 1)

/* ------------------------------------------------------------------
 * Scancodes
 * ------------------------------------------------------------------ */
#define SC_EXTENDED           0xE0
#define SCANCODE_RELEASE_MASK 0x80

#define SC_LSHIFT             0x2A
#define SC_RSHIFT             0x36
#define SC_LCTRL              0x1D
#define SC_LALT               0x38
#define SC_RCTRL              0x1D
#define SC_RALT               0x38

#define SC_ARROW_UP           0x48
#define SC_ARROW_DOWN         0x50
#define SC_ARROW_LEFT         0x4B
#define SC_ARROW_RIGHT        0x4D

#define SC_F1                 0x3B
#define SC_F2                 0x3C
#define SC_F3                 0x3D
#define SC_F4                 0x3E
#define SC_F5                 0x3F
#define SC_F6                 0x40
#define SC_F7                 0x41
#define SC_F8                 0x42
#define SC_F9                 0x43
#define SC_F10                0x44
#define SC_F11                0x57
#define SC_F12                0x58

/* ------------------------------------------------------------------
 * Scancode → ASCII tables
 * ------------------------------------------------------------------ */
static const char SCANCODE_LOWER[] = {
        0, 0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
        'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,'a','s',
        'd','f','g','h','j','k','l',';','\'','`',0,'\\','z','x','c','v',
        'b','n','m',',','.','/',0,'*',0,' '
};

static const char SCANCODE_UPPER[] = {
        0, 0, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
        'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,'A','S',
        'D','F','G','H','J','K','L',':','"','~',0,'|','Z','X','C','V',
        'B','N','M','<','>','?',0,'*',0,' '
};

/* ------------------------------------------------------------------
 * Internal state
 * ------------------------------------------------------------------ */
static bool shift_pressed = false;
static bool ctrl_pressed  = false;
static bool alt_pressed   = false;
static bool extended_code = false;

/* User-supplied handler */
static void (*keyboard_handler_cb)(char value,
                                   enum keyboard_code code,
                                   bool ctrl,
                                   bool alt,
                                   bool shift) = NULL;

/* ------------------------------------------------------------------
 * I/O helpers
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
 * Helper to emit key events with modifier state
 * ------------------------------------------------------------------ */
static void emit_key(char value, enum keyboard_code code)
{
    if (keyboard_handler_cb != NULL)
    {
        keyboard_handler_cb(value, code,
                            ctrl_pressed, alt_pressed, shift_pressed);
    }
}

/* ------------------------------------------------------------------
 * Ordinary C IRQ handler (no IRQ mechanics)
 * ------------------------------------------------------------------ */
void keyboard_interrupt_handler(void)
{
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);

    if (scancode == SC_EXTENDED)
    {
        extended_code = true;
        return;
    }

    if (extended_code)
    {
        extended_code = false;
        bool is_release = (scancode & SCANCODE_RELEASE_MASK);
        uint8_t base = scancode & ~SCANCODE_RELEASE_MASK;

        switch (base)
        {
            case SC_RCTRL:  ctrl_pressed = !is_release; if (!is_release) emit_key(0, KEY_CTRL); break;
            case SC_RALT:   alt_pressed  = !is_release; if (!is_release) emit_key(0, KEY_ALT);  break;
            case SC_ARROW_UP:    if (!is_release) emit_key(0, KEY_UP);    break;
            case SC_ARROW_DOWN:  if (!is_release) emit_key(0, KEY_DOWN);  break;
            case SC_ARROW_LEFT:  if (!is_release) emit_key(0, KEY_LEFT);  break;
            case SC_ARROW_RIGHT: if (!is_release) emit_key(0, KEY_RIGHT); break;
            default: break;
        }
        return;
    }

    {
        bool is_release = (scancode & SCANCODE_RELEASE_MASK);
        uint8_t base = scancode & ~SCANCODE_RELEASE_MASK;

        switch (base)
        {
            case SC_LSHIFT:
            case SC_RSHIFT:
                shift_pressed = !is_release;
                if (!is_release) emit_key(0, KEY_SHIFT);
                return;

            case SC_LCTRL:
                ctrl_pressed = !is_release;
                if (!is_release) emit_key(0, KEY_CTRL);
                return;

            case SC_LALT:
                alt_pressed = !is_release;
                if (!is_release) emit_key(0, KEY_ALT);
                return;

            case SC_F1: case SC_F2: case SC_F3: case SC_F4: case SC_F5: case SC_F6:
            case SC_F7: case SC_F8: case SC_F9: case SC_F10: case SC_F11: case SC_F12:
                if (!is_release)
                {
                    enum keyboard_code code = KEY_NONE;
                    switch (base)
                    {
                        case SC_F1:  code = KEY_F1;  break;
                        case SC_F2:  code = KEY_F2;  break;
                        case SC_F3:  code = KEY_F3;  break;
                        case SC_F4:  code = KEY_F4;  break;
                        case SC_F5:  code = KEY_F5;  break;
                        case SC_F6:  code = KEY_F6;  break;
                        case SC_F7:  code = KEY_F7;  break;
                        case SC_F8:  code = KEY_F8;  break;
                        case SC_F9:  code = KEY_F9;  break;
                        case SC_F10: code = KEY_F10; break;
                        case SC_F11: code = KEY_F11; break;
                        case SC_F12: code = KEY_F12; break;
                        default: break;
                    }
                    emit_key(0, code);
                }
                return;

            default:
                break;
        }

        if (is_release || base >= sizeof(SCANCODE_LOWER))
            return;

        char c = shift_pressed ? SCANCODE_UPPER[base] : SCANCODE_LOWER[base];
        if (!c)
            return;

        if (ctrl_pressed)
        {
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 1);
            else if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 1);
        }

        emit_key(c, KEY_NONE);
    }
}

/* ------------------------------------------------------------------
 * Arch stub for this IRQ vector
 * ------------------------------------------------------------------ */
MAKE_IRQ_STUB(keyboard_irq_stub, 9)

/* ------------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------------ */
void keyboard_init(void (*handler)(char value,
                                   enum keyboard_code code,
                                   bool ctrl,
                                   bool alt,
                                   bool shift))
{
    keyboard_handler_cb = handler;

    shift_pressed = false;
    ctrl_pressed  = false;
    alt_pressed   = false;
    extended_code = false;

    /* Register C handler in the common table */
    irq_register_handler(KEYBOARD_IRQ_VECTOR, keyboard_interrupt_handler);

    /* Install the arch stub into the IDT */
    idt_set_gate(KEYBOARD_IRQ_VECTOR, (uint32_t)keyboard_irq_stub, 0x08, 0x8E);

    /* Unmask IRQ1 on master PIC */
    uint8_t mask = inb(PIC1_DATA_PORT);
    mask &= (uint8_t)~KEYBOARD_IRQ_MASK;
    outb(PIC1_DATA_PORT, mask);
}
