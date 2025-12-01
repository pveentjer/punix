#include "sched.h"

void run_queue_init(struct run_queue *run_queue)
{
    run_queue->len = 0;
    run_queue->first = NULL;
    run_queue->last = NULL;
}

void run_queue_push(struct run_queue *run_queue, struct task_struct *task)
{
    if (run_queue->len == 0)
    {
        run_queue->first = task;
        run_queue->last = task;
        run_queue->len = 1;
    }
    else
    {
        run_queue->last->next = task;
        run_queue->last = task;
        run_queue->len++;
    }
}

struct task_struct *run_queue_poll(struct run_queue *run_queue)
{
    size_t len = run_queue->len;
    if (len == 0)
    {
        return NULL;
    }
    else if (len == 1)
    {
        struct task_struct *task = run_queue->first;
        run_queue->first = NULL;
        run_queue->last = NULL;
        run_queue->len = 0;
        return task;
    }
    else
    {
        struct task_struct *task = run_queue->first;
        run_queue->first = task->next;
        run_queue->len--;
        task->next=NULL;
        return task;
    }
}