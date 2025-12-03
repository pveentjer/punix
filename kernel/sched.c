// sched.c
#include "sched.h"
#include "vga.h"
#include "interrupt.h"

#include "process.h"

struct struct_sched 
{
    struct run_queue run_queue;
    struct task_struct *current;
} sched;

/* process table provided by linker.ld */
extern const struct process_desc __proctable_start[];
extern const struct process_desc __proctable_end[];

struct struct_sched 
{
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

#define TASK_STACK_SIZE 4096

uint32_t next_stack = 16*1024;

#define MAX_TASK_CNT 1024

struct task_struct task_struct_slab[MAX_TASK_CNT];

size_t task_struct_slab_next = 0;

static int kstrcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) 
    {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

static const struct process_desc *find_process(const char *name)
{
    const struct process_desc *p = __proctable_start;
    while (p < __proctable_end) 
    {
        if (kstrcmp(p->name, name) == 0) 
        {
            return p;
        }
        p++;
    }
    return 0;
}

void exit(int status)
{
    struct task_struct *current = sched.current;
    if (current == NULL)
    {
        panic("exit failed because there is no current task.\n");
    }

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

void task_trampoline(int (*entry)(int, char**), int argc, char **argv) 
{
    int status = entry(argc, argv);
    exit(status);
}

int sched_get_tasks(struct task_info *tasks, int max)
{
    int count = 0;

    if (sched.current && count < max)
    {
        tasks[count].pid = sched.current->pid;
        for (int i = 0; i < MAX_NAME_LENGTH; i++)
        {
            tasks[count].name[i] = sched.current->name[i];
            if (sched.current->name[i] == '\0')
            {
                break;
            }
        }
        count++;
    }

    struct task_struct *t = sched.run_queue.first;
    while (t && count < max)
    {
        tasks[count].pid = t->pid;
        for (int i = 0; i < MAX_NAME_LENGTH; i++)
        {
            tasks[count].name[i] = t->name[i];
            if (t->name[i] == '\0')
            {
                break;
            }
        }
        count++;
        t = t->next;
    }

    return count;
}

void sched_add_task(const char *filename, int argc, char **argv)
{
    if (task_struct_slab_next >= MAX_TASK_CNT) {
        panic("sched_add_task: too many tasks");
    }

    const struct process_desc *desc = find_process(filename);
    if (!desc) {
        screen_print("sched_add_task: unknown process '");
        screen_print(filename);
        screen_println("'");
        return;
    }

    struct task_struct *task = &task_struct_slab[task_struct_slab_next++];
    task->pid = next_pid++;

    for (int i = 0; i < MAX_NAME_LENGTH; i++)
    {
        task->name[i] = filename[i];
        if (filename[i] == '\0') break;
    }

    uint32_t stack_top = next_stack;
    next_stack += TASK_STACK_SIZE;

    char *sp = (char *)stack_top;

    // Copy strings onto stack, remember start position
    char *strings_start = sp;
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = 0;
        while (argv[i][len]) len++;
        len++;
        
        sp -= len;
        for (size_t j = 0; j < len; j++) {
            sp[j] = argv[i][j];
        }
    }

    // Align
    sp = (char *)((uint32_t)sp & ~3);
    uint32_t *sp32 = (uint32_t *)sp;

    // Build argv array - walk through strings again
    *(--sp32) = 0;
    char *str_ptr = (char *)stack_top;
    for (int i = argc - 1; i >= 0; i--) {
        size_t len = 0;
        while (argv[i][len]) len++;
        len++;
        str_ptr -= len;
        *(--sp32) = (uint32_t)str_ptr;
    }
    char **new_argv = (char **)sp32;

    *(--sp32) = (uint32_t)new_argv;
    *(--sp32) = (uint32_t)argc;
    *(--sp32) = (uint32_t)desc->entry;
    *(--sp32) = 0;
    *(--sp32) = (uint32_t)task_trampoline;
    *(--sp32) = 0x202;
    *(--sp32) = 0;
    *(--sp32) = 0;
    *(--sp32) = 0;
    *(--sp32) = 0;

    task->esp = (uint32_t)sp32;
    task->eip = (uint32_t)task_trampoline;
    task->eflags = 0x202;
    task->parent = sched.current ? sched.current : task;

    run_queue_push(&sched.run_queue, task);
}

void sched_start(void)
{
    struct task_struct dummy;
    sched.current = run_queue_poll(&sched.run_queue);
    if (!sched.current)
    {
        panic("sched_start can't start with an empty run_queue\n");
    }

    task_context_switch(&dummy, sched.current);
}

pid_t getpid(void)
{
    return sched.current->pid;
}

void yield(void)
{
 
    // if(!interrupts_are_enabled())
    // {
    //     screen_println("yield; interrupts not enabled.");    
    // }

    if (sched.run_queue.len == 0)
    {
        // screen_println("yield; no other task.");
        return;
    }

    struct task_struct *prev = sched.current;
    run_queue_push(&sched.run_queue, prev);

    sched.current = run_queue_poll(&sched.run_queue);
    struct task_struct *next = sched.current;

    task_context_switch(prev, next);
}