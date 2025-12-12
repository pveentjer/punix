//
// Created by pveentjer on 12/12/25.
//

#ifndef WAIT_H
#define WAIT_H

#include <stddef.h>

struct task;

struct wait_queue_entry{
    struct task *task;
    struct wait_queue_entry *next;
};

struct wait_queue{
    struct wait_queue_entry *head;
};

void wait_queue_init(struct wait_queue *queue);

void wait_queue_add(struct wait_queue *queue, struct wait_queue_entry *entry);

void wakeup(struct wait_queue *queue);

#endif //WAIT_H
