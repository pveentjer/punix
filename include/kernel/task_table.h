#ifndef TASK_TABLE_H
#define TASK_TABLE_H

#include "sched.h"

struct task_slot
{
    struct task_struct task;
    uint32_t generation;
};

struct task_table
{
    uint32_t free_ring[MAX_PROCESS_CNT];
    // the side where for allocation
    uint32_t free_head;
    // the side for free
    uint32_t free_tail;
    struct task_slot slots[MAX_PROCESS_CNT];
};

struct task_struct *task_table_find_task_by_pid(
        const struct task_table *task_table,
        const pid_t pid);

void task_table_init(struct task_table *task_table);

struct task_struct *task_table_alloc(struct task_table *task_table);

void task_table_free(
        struct task_table *task_table,
        struct task_struct *task);

#endif /* TASK_TABLE_H */
