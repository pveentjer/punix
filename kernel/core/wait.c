#include "kernel/wait.h"
#include "kernel/sched.h"

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

        if (task != NULL &&
            (task->state == TASK_INTERRUPTIBLE || task->state == TASK_UNINTERRUPTIBLE))
        {
            sched_enqueue(task);
        }

        entry = next;
    }
}

void wait_event(struct wait_queue *queue, bool (*cond)(void *obj), void *ctx, wait_mode wait_mode)
{
//    kprintf("wait event\n");

    if (queue == NULL || cond == NULL)
    {
        return;
    }

    struct wait_queue_entry wait_entry;
    struct task *current = sched_current();

    wait_queue_entry_init(&wait_entry, current);

    while (!cond(ctx))
    {
        struct task *task = current;

        task->state = (wait_mode == WAIT_INTERRUPTIBLE)
                      ? TASK_INTERRUPTIBLE
                      : TASK_UNINTERRUPTIBLE;

        wait_queue_add(queue, &wait_entry);

        sched_schedule();

        wait_queue_remove(&wait_entry);

        if (current->signal.pending != 0u && wait_mode == WAIT_INTERRUPTIBLE)
        {
            sched_exit(-1);
        }
    }
}