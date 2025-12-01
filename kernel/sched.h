#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>

struct task_struct
{
    uint32_t pid;
    uint32_t eip;        // entry function
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
    int      started;    // 0 = never run, 1 = running/suspended
    struct task_struct *next;
};

struct run_queue
{
    struct task_struct *first;
    struct task_struct *last;
    size_t len;
};

extern struct run_queue run_queue;
extern struct task_struct *current;

void run_queue_init(struct run_queue *run_queue);
void run_queue_push(struct run_queue *run_queue, struct task_struct *task);
struct task_struct *run_queue_poll(struct run_queue *run_queue);

void sched_init(void);
void sched_add_task(struct task_struct *task);
void sched_start(void);
void yield(void);

/* asm functions */
void task_start(struct task_struct *t);
int  task_context_switch(struct task_struct *current, struct task_struct *next);

#endif // SCHED_H
