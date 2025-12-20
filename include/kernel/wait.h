//
// Created by pveentjer on 12/12/25.
//

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

void wait_queue_init(struct wait_queue *queue);

void wait_queue_entry_init(
        struct wait_queue_entry *entry,
        struct task *task);

void wait_queue_add(
        struct wait_queue *queue,
        struct wait_queue_entry *entry);

void wait_queue_remove(
        struct wait_queue_entry *entry);

void wakeup(struct wait_queue *queue);

void wait_event(struct wait_queue *queue,bool (*cond)(void *obj), void *ctx);

#endif //WAIT_H
