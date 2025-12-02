#include "keyboard.h"
#include "vga.h"
#include "interrupt.h"
#include <stddef.h>
#include <stdbool.h>

#define KEYBOARD_DATA_PORT 0x60
#define PIC1_COMMAND       0x20
#define PIC1_DATA          0x21

static bool shift_pressed = false;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ------------------------------------------------------------------
 * Simple ring buffer for keyboard characters
 * ------------------------------------------------------------------ */

#define KBD_BUF_SIZE 128

static char kbd_buf[KBD_BUF_SIZE];
static volatile size_t kbd_head = 0;  // write index
static volatile size_t kbd_tail = 0;  // read index

static void kbd_buffer_put(char c)
{
    size_t next_head = (kbd_head + 1) % KBD_BUF_SIZE;

    // If buffer is full, drop the character (simple behavior for now)
    if (next_head == kbd_tail) {
        return;
    }

    kbd_buf[kbd_head] = c;
    kbd_head = next_head;
}

bool keyboard_has_char(void)
{
    return kbd_head != kbd_tail;
}

char keyboard_get_char(void)
{
    // Simple blocking/spin wait until a character is available.
    // For now this is fine; later you can integrate with your scheduler.
    while (!keyboard_has_char()) {
        // spin
    }

    char c = kbd_buf[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUF_SIZE;
    return c;
}

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

void keyboard_interrupt_handler(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    
    // Handle shift keys
    if (scancode == 0x2A || scancode == 0x36) {
        shift_pressed = true;
    } else if (scancode == 0xAA || scancode == 0xB6) {
        shift_pressed = false;
    }
    // Handle regular keys (only key press, not release)
    else if (!(scancode & 0x80) && scancode < sizeof(scancode_lower)) {
        char c = shift_pressed ? scancode_upper[scancode] : scancode_lower[scancode];
        if (c != 0) {
            // Instead of printing, buffer the character
            kbd_buffer_put(c);
        }
    }
    
    // Send EOI to PIC
    outb(PIC1_COMMAND, 0x20);
}

// Assembly ISR stub
__attribute__((naked))
void keyboard_isr(void) {
    asm volatile(
        "pushal\n\t"
        "call keyboard_interrupt_handler\n\t"
        "popal\n\t"
        "iret"
    );
}

void keyboard_init(void) {
    // Register keyboard interrupt handler at IRQ1 (vector 0x09)
    idt_set_gate(0x09, (uint32_t)keyboard_isr, 0x08, 0x8E);
    
    // Enable IRQ1 on PIC
    uint8_t mask = inb(PIC1_DATA);
    mask &= ~0x02;  // Clear bit 1 (IRQ1)
    outb(PIC1_DATA, mask);
}
