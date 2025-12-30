#ifndef WAIT_H
#define WAIT_H

#include <stddef.h>
#include <stdbool.h>

struct task;


struct wait_queue;

struct wait_queue_entry
{
    struct task *task;

    struct wait_queue_entry *prev;
    struct wait_queue_entry *next;

    struct wait_queue *queue;
};

struct wait_queue
{
    struct wait_queue_entry *head;
    struct wait_queue_entry *tail;
};

typedef enum wait_mode
{
    WAIT_UNINTERRUPTIBLE = 0,
    WAIT_INTERRUPTIBLE   = 1,
} wait_mode;

void wait_queue_init(struct wait_queue *queue);

void wait_queue_entry_init(struct wait_queue_entry *entry, struct task *task);

void wait_queue_add(struct wait_queue *queue, struct wait_queue_entry *entry);

void wait_queue_remove(struct wait_queue_entry *entry);

void wakeup(struct wait_queue *queue);

void wait_event(struct wait_queue *queue, bool (*cond)(void *obj), void *ctx, wait_mode wait_mode);

#endif //WAIT_H
