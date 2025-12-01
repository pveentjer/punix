// sched.h
#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>

struct task_struct
{
    uint32_t pid;
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
    struct task_struct *next;
};

struct run_queue
{
    struct task_struct *first;
    struct task_struct *last;
    size_t len;
};

void run_queue_init(struct run_queue *run_queue);

void run_queue_push(struct run_queue *run_queue, struct task_struct* task);

struct task_struct* run_queue_poll(struct run_queue* run_queue);

#endif // SCHED_H
