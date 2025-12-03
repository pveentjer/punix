#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdint.h"
#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdbool.h"

/* Initialize the keyboard driver and enable IRQ1 */
void keyboard_init(void);

/* Returns true if at least one character is waiting in the buffer */
bool keyboard_has_char(void);

/* Blocks (or spins) until a character is available, then returns it */
char keyboard_get_char(void);

#endif /* KEYBOARD_H */
