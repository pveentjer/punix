// sched.c
#include "sched.h"
#include "vga.h"
#include "interrupt.h"

#include "process.h"

struct struct_sched {
    struct run_queue run_queue;
    struct task_struct *current;
} sched;

/* process table provided by linker.ld */
extern const struct process_desc __proctable_start[];
extern const struct process_desc __proctable_end[];

struct struct_sched {
    struct run_queue run_queue;
    struct task_struct *current;
} sched;


void panic(char* msg)
{
    screen_println("Kernel Panic!!!");
    screen_println(msg);
    
    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

/* asm functions */
void task_prepare_new(struct task_struct *t);

int  task_context_switch(struct task_struct *current, struct task_struct *next);


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
    sched.current = NULL;
    run_queue_init(&sched.run_queue);
}

uint32_t next_pid = 0;

#define STACK_SIZE 4096

uint32_t next_stack = 16*1024;

#define MAX_TASK 1024

struct task_struct task_struct_slab[MAX_TASK];

size_t task_struct_slab_next = 0;

static int kstrcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const struct process_desc *find_process(const char *name)
{
    const struct process_desc *p = __proctable_start;
    while (p < __proctable_end) {
        if (kstrcmp(p->name, name) == 0) {
            return p;
        }
        p++;
    }
    return 0;
}


void sched_add_task(const char *filename)
{
    if (task_struct_slab_next >= MAX_TASK) {
        panic("sched_add_task: too many tasks");
    }

    const struct process_desc *desc = find_process(filename);
    if (!desc) {
        screen_print("sched_add_task: unknown process '");
        screen_print(filename);
        screen_println("'");
        return; // or panic(...) if you prefer hard fail
    }

    struct task_struct *task = &task_struct_slab[task_struct_slab_next++];

    task->pid = next_pid++;

    /* Allocate stack */
    uint32_t sp = next_stack;
    next_stack += STACK_SIZE;

    task->eip = (uint32_t)desc->entry;  // <-- use the registered entry point
    task->esp = sp;
    task->ebp = sp;
    task->next = NULL;

    if (sched.current == NULL) {
        // the root process points to itself.
        task->parent = task;
    } else {
        task->parent = sched.current;
    }

    // Prepare the new task's stack so it looks like it was context-switched
    task_prepare_new(task);
    run_queue_push(&sched.run_queue, task);
}

void exit(int status)
{
    struct task_struct *current = sched.current;
    if(current == NULL)
    {
        panic("exit failed because there is no current task.\n");
    }

    screen_print("Exit: ");
    screen_put_uint64(current->pid);
    screen_put_char('\n');

    screen_print("run_queue_len: ");
    screen_put_uint64(sched.run_queue.len);
    screen_put_char('\n');


    struct task_struct *next = run_queue_poll(&sched.run_queue);

    if (next == NULL)
    {
        sched.current = NULL;

        screen_println("Halted!");
        for (;;)
        {
            __asm__ volatile("hlt");
        }
    }
    else
    {
        sched.current = next;
        task_context_switch(current, next);
    }
}


void sched_start(void)
{
    struct task_struct dummy;
    sched.current = run_queue_poll(&sched.run_queue);
    if (!sched.current)
    {
        panic("sched_start can't start with an empt run_queue\n");
    }

    task_context_switch(&dummy, sched.current);
}

void yield(void)
{
    if(!interrupts_are_enabled())
    {
        screen_println("yield; interrupts not enabled.");    
    }

    if (sched.run_queue.len == 0)
    {
        screen_println("yield; no other task.");
        return;
    }

    struct task_struct *prev = sched.current;
    run_queue_push(&sched.run_queue, prev);

    sched.current = run_queue_poll(&sched.run_queue);
    struct task_struct *next = sched.current;

    task_context_switch(prev, next);
}