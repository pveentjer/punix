#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include <stddef.h>

/**
 * Initialize the keyboard driver and enable keyboard interrupts (IRQ1).
 */
void keyboard_init(void);

/**
 * Read up to `maxlen` characters from the keyboard input buffer into `buf`.
 * Returns the number of characters copied.
 *
 * - Non-blocking: returns 0 if no characters are available.
 * - Handles Shift, Ctrl, and Alt modifiers.
 */
size_t keyboard_read(char *buf, size_t maxlen);

#endif /* KERNEL_KEYBOARD_H */
