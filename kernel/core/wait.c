#include "../../include/kernel/wait.h"
#include "../../include/kernel/sched.h"

void wait_queue_init(struct wait_queue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    queue->head = NULL;
    queue->tail = NULL;
}

void wait_queue_entry_init(struct wait_queue_entry *entry, struct task *task)
{
    if (entry == NULL)
    {
        return;
    }

    entry->task = task;
    entry->prev = NULL;
    entry->next = NULL;
    entry->queue = NULL;
}

void wait_queue_add(struct wait_queue *queue, struct wait_queue_entry *entry)
{
    if (queue == NULL || entry == NULL)
    {
        return;
    }

    /* already queued => no-op (prevents double insert corruption) */
    if (entry->queue != NULL)
    {
        return;
    }

    entry->queue = queue;
    entry->prev = queue->tail;
    entry->next = NULL;

    if (queue->tail != NULL)
    {
        queue->tail->next = entry;
    }
    else
    {
        queue->head = entry;
    }

    queue->tail = entry;
}

void wait_queue_remove(struct wait_queue_entry *entry)
{
    if (entry == NULL)
    {
        return;
    }

    struct wait_queue *queue = entry->queue;
    if (queue == NULL)
    {
        return; /* not queued */
    }

    if (entry->prev != NULL)
    {
        entry->prev->next = entry->next;
    }
    else
    {
        queue->head = entry->next;
    }

    if (entry->next != NULL)
    {
        entry->next->prev = entry->prev;
    }
    else
    {
        queue->tail = entry->prev;
    }

    entry->prev = NULL;
    entry->next = NULL;
    entry->queue = NULL;
}

void wakeup(struct wait_queue *queue)
{
    if (queue == NULL)
    {
        return;
    }

    struct wait_queue_entry *entry = queue->head;

    /* detach whole list first */
    queue->head = NULL;
    queue->tail = NULL;

    while (entry != NULL)
    {
        struct wait_queue_entry *next = entry->next;
        struct task *task = entry->task;

        /* fully detach entry */
        entry->prev = NULL;
        entry->next = NULL;
        entry->queue = NULL;

        if (task != NULL && task->state == TASK_BLOCKED)
        {
            sched_enqueue(task);
        }

        entry = next;
    }

    sched_schedule();
}

void wait_event(struct wait_queue *queue, bool (*cond)(void *obj), void *ctx)
{
    if (queue == NULL || cond == NULL)
    {
        return;
    }

    struct wait_queue_entry wait_entry;
    wait_queue_entry_init(&wait_entry, sched_current());

    while (!cond(ctx))
    {
        struct task *task = sched_current();
        task->state = TASK_BLOCKED;

        wait_queue_add(queue, &wait_entry);

        sched_schedule();

        wait_queue_remove(&wait_entry);

        if (t->pending_signals != 0u)
        {
            sched_exit(-1);
        }
    }
}