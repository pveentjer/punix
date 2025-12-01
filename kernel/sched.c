// sched.c
#include "sched.h"
#include "screen.h"

struct run_queue run_queue;
struct task_struct *current;

/* ---------------- Run queue ---------------- */

void run_queue_init(struct run_queue *rq)
{
    rq->len = 0;
    rq->first = NULL;
    rq->last = NULL;
}

void run_queue_push(struct run_queue *rq, struct task_struct *task)
{
    task->next = NULL;
    if (rq->len == 0)
    {
        rq->first = rq->last = task;
    }
    else
    {
        rq->last->next = task;
        rq->last = task;
    }
    rq->len++;
}

struct task_struct *run_queue_poll(struct run_queue *rq)
{
    if (rq->len == 0)
    {
        return NULL;
    }

    struct task_struct *t = rq->first;
    rq->first = t->next;
    rq->len--;
    if (rq->len == 0)
    {
        rq->last = NULL;
    }
    t->next = NULL;
    return t;
}

/* ---------------- Scheduler ---------------- */

void sched_init(void)
{
    current = NULL;
    run_queue_init(&run_queue);
}

/* Caller sets: pid, eip, esp, ebp, eflags, next=NULL */
void sched_add_task(struct task_struct *task)
{
    task->started = 0;
    run_queue_push(&run_queue, task);

    screen_print("add task, rq.len:");
    screen_put_uint64(run_queue.len);
    screen_put_char('\n');
}

void sched_start(void)
{
    current = run_queue_poll(&run_queue);
    if (!current)
    {
        for (;;)
            __asm__ volatile("hlt");
    }

    current->started = 1;
    task_start(current);     // never returns
}

void yield(void)
{
    if (run_queue.len == 0)
    {
        screen_println("yield; no other task.");
        return;
    }

    struct task_struct *prev = current;
    run_queue_push(&run_queue, prev);

    current = run_queue_poll(&run_queue);
    struct task_struct *next = current;

    if (!next->started)
    {
        screen_println("yield; other new task");

        next->started = 1;
        
        // Prepare the new task's stack so it looks like it was context-switched
        task_prepare_new(next);
        
        // Now do a normal context switch
        task_context_switch(prev, next);
    }
    else
    {
        screen_println("yield; other old task.");

        task_context_switch(prev, next); // resume saved context
    }
}