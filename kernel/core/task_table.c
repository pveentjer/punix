#include "../../include/kernel/task_table.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/constants.h"

#define PID_MASK (MAX_PROCESS_CNT -1)

#define UNUSED_PID -1

#define PID_MAX INT_MAX

#define MAX_ROUND (PID_MAX / MAX_PROCESS_CNT)


void task_table_init(struct task_table *task_table)
{
    task_table->free_head = 0;
    task_table->free_tail = MAX_PROCESS_CNT;

    for (int slot_idx = 0; slot_idx < MAX_PROCESS_CNT; slot_idx++)
    {
        struct task_slot *slot = &task_table->slots[slot_idx];
        slot->round = 0;

        struct task_struct *task = &slot->task;
        memset(task, 0, sizeof(struct task_struct));
        task->pid = UNUSED_PID;
        task->mem_base = PROCESS_BASE + slot_idx * PROCESS_SIZE;
        task_table->free_ring[slot_idx] = slot_idx;
    }
}

struct task_struct *task_table_find_task_by_pid(
        const struct task_table *task_table,
        const pid_t pid)
{
    if (pid < 0)
    {
        return NULL;
    }

    uint32_t slot_idx = pid & PID_MASK;
    struct task_struct *task = &task_table->slots[slot_idx].task;
    if (task->pid != pid)
    {
        return NULL;
    }
    else
    {
        return task;
    }
}

struct task_struct *task_table_alloc(struct task_table *task_table)
{
    if (task_table->free_head == task_table->free_tail)
    {
        return NULL;
    }

    const uint32_t free_ring_idx = (task_table->free_head) & PID_MASK;
    const uint32_t slot_idx = task_table->free_ring[free_ring_idx];

    struct task_slot *slot = &task_table->slots[slot_idx];
    struct task_struct *task = &slot->task;

    task->pid = slot->round * MAX_PROCESS_CNT + slot_idx;
    // we need to take care of the wrap around of the slot->round
    slot->round = (slot->round + 1) & MAX_ROUND;
    task_table->free_head++;

    return task;
}

void task_table_free(
        struct task_table *task_table,
        struct task_struct *task_struct)
{
    if (task_table->free_tail - task_table->free_head == MAX_PROCESS_CNT)
    {
        panic("task_table_free: too many frees.");
    }

    const pid_t pid = task_struct->pid;
    if (pid < 0)
    {
        panic("task_table_free: task has negative pid.");
    }

    const uint32_t slot_idx = pid & PID_MASK;
    const uint32_t free_ring_idx = task_table->free_tail & PID_MASK;

    struct task_slot *slot = &task_table->slots[slot_idx];
    if (&slot->task != task_struct)
    {
        panic("task_table_free: task pointer/slot mismatch");
    }

    task_table->free_ring[free_ring_idx] = slot_idx;
    task_table->free_tail++;
    task_struct->pid = UNUSED_PID;
}

