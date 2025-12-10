#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stddef.h>
#include "console.h"

#define TTY_INPUT_BUF_SIZE 256u
#define TTY_OUTPUT_BUF_SIZE 4096u

struct tty
{
    char   in_buf[TTY_INPUT_BUF_SIZE];
    size_t in_head;
    size_t in_tail;

    char   out_buf[TTY_OUTPUT_BUF_SIZE];
    size_t out_head;
    size_t out_tail;

    struct console *console;
};

extern struct tty tty0;

void tty_init(struct tty *tty);

void tty_input_put(struct tty *tty, char c);

size_t tty_read(struct tty *tty, char *buf, size_t maxlen);

size_t tty_write(struct tty *tty, char *buf, size_t maxlen);

void tty_system_init(void);

#endif /* KERNEL_TTY_H */
