#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stddef.h>

/* ------------------------------------------------------------------
 * Basic TTY with a single circular input buffer.
 * ------------------------------------------------------------------ */

#define TTY_INPUT_BUF_SIZE 256u

struct tty
{
    char   in_buf[TTY_INPUT_BUF_SIZE];
    size_t in_head;   /* monotonic write index */
    size_t in_tail;   /* monotonic read index */
};

/**
 * Global primary TTY instance (for now only one).
 */
extern struct tty tty0;

/**
 * Initialize a TTY instance.
 */
void tty_init(struct tty *tty);

/**
 * Push a single input character into the TTY input buffer.
 * If the buffer is full, the NEWEST character is dropped.
 */
void tty_input_put(struct tty *tty, char c);

/**
 * Read up to maxlen characters from the TTY input buffer.
 * Returns number of characters copied.
 * Non-blocking: returns 0 if no data is available.
 */
size_t tty_read(struct tty *tty, char *buf, size_t maxlen);

/**
 * Initialize the TTY "subsystem":
 *   - initialize tty0
 *   - configure the keyboard with the proper callback handler
 */
void tty_system_init(void);

#endif /* KERNEL_TTY_H */
