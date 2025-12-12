#ifndef KERNEL_TTY_H
#define KERNEL_TTY_H

#include <stddef.h>
#include "console.h"
#include "constants.h"
#include "wait.h"

struct tty
{
    size_t idx;

    char in_buf[TTY_INPUT_BUF_SIZE];
    size_t in_head;
    size_t in_tail;

    char out_buf[TTY_OUTPUT_BUF_SIZE];
    size_t out_head;
    size_t out_tail;

    size_t cursor_pos;

    struct console *console;

    struct wait_queue in_wait_queue;
};

void tty_system_init(void);

void tty_activate(size_t idx);

struct tty *tty_active(void);

void tty_input_put(struct tty *tty, char c);

struct tty *tty_get(size_t idx);

size_t tty_read(struct tty *tty, char *buf, size_t maxlen);

size_t tty_write(struct tty *tty, const char *buf, size_t maxlen);

#endif /* KERNEL_TTY_H */
