#ifndef KERNEL_KEYBOARD_H
#define KERNEL_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

/* ------------------------------------------------------------------
 * Symbolic key codes for non-printable keys.
 * Printable keys are passed via 'value' (ASCII).
 * ------------------------------------------------------------------ */
enum keyboard_code
{
    KEY_NONE = 0,
    KEY_CTRL,
    KEY_ALT,
    KEY_SHIFT,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
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
 *                   - ctrl:  true if Ctrl is currently pressed
 *                   - alt:   true if Alt  is currently pressed
 *                   - shift: true if Shift is currently pressed
 */
void keyboard_init(void (*handler)(char value,
                                   enum keyboard_code code,
                                   bool ctrl,
                                   bool alt,
                                   bool shift));

#endif /* KERNEL_KEYBOARD_H */
