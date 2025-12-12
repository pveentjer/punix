#include "../../include/kernel/wait.h"
#include "kernel/sched.h"

void wait_queue_init(struct wait_queue *queue)
{
    queue->head = NULL;
}

void wait_queue_add(struct wait_queue *queue, struct wait_queue_entry *entry)
{
    entry->next = queue->head;
    queue->head = entry;
}

void wakeup(struct wait_queue *queue)
{
    struct wait_queue_entry *entry = queue->head;
    queue->head = NULL;

    while (entry != NULL)
    {
        struct wait_queue_entry *next = entry->next;
        entry->next = NULL;
        struct task *task = entry->task;

        task->state = TASK_QUEUED;

        sched_enqueue(task);

        entry = next;
    }

    sched_schedule();
}
