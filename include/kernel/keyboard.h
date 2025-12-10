#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include <stdint.h>

enum keyboard_code
{
    KEY_NONE = 0,
    KEY_CTRL, KEY_ALT, KEY_SHIFT,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6,
    KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_F11, KEY_F12
};

/**
 * Initialize the keyboard driver.
 *
 * @param handler  Callback invoked for each key press.
 *                 The callback receives:
 *                   - value: printable ASCII (0 if none)
 *                   - code:  symbolic key code (KEY_*)
 */
void keyboard_init(void (*handler)(char value, enum keyboard_code code));

#endif /* KERNEL_KEYBOARD_H */
