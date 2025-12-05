// sched.c
#include "../../include/kernel/sched.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/task_slab.h"
#include "../../include/kernel/constants.h"

struct struct_sched
{
    struct run_queue run_queue;
    struct task_struct *current;
} sched;


struct task_struct *pid_table[MAX_PROCESS_CNT];

int task_context_switch(struct task_struct *current, struct task_struct *next);


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
    k_memset(pid_table, 0, sizeof(pid_table));

    sched.current = NULL;
    run_queue_init(&sched.run_queue);
}

void sched_exit(int status)
{

    struct task_struct *current = sched.current;
    if (current == NULL)
    {
        panic("exit failed because there is no current task.\n");
    }

    pid_table[current->pid] = NULL;

    // A copy of the current needs to be made because it is needed
    // for the context switch at the end, but we want to return the
    // struct to the slab before that.
    struct task_struct copy_current = *current;

    task_slab_free(current);

    struct task_struct *next = run_queue_poll(&sched.run_queue);

    if (next == NULL)
    {
        sched.current = NULL;

        screen_printf("Halted!\n");
        for (;;)
        {
            __asm__ volatile("hlt");
        }
    }
    else
    {
        sched.current = next;
        task_context_switch(&copy_current, next);
    }
}

void task_trampoline(int (*entry)(int, char **), int argc, char **argv)
{
//    screen_printf("task_trampoline: enter\n");

    int status = entry(argc, argv);
//    if (status!= 0)
//    {
//            screen_printf("task_trampoline: returned from main\nexit status = %d\n", status);
//    }

//    screen_printf("task_trampoline: returned from main\nexit status = %d\n", status);


    sched_exit(status);
}


int sched_get_tasks(struct task_info *tasks, int max)
{
    int count = 0;

    if (sched.current && count < max)
    {
        tasks[count].pid = sched.current->pid;
        for (int i = 0; i < MAX_PROCESS_NAME_LENGTH; i++)
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
        for (int i = 0; i < MAX_PROCESS_NAME_LENGTH; i++)
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

pid_t sched_add_task(const char *filename, int argc, char **argv)
{
    const struct embedded_app *app = find_app(filename);
    if (!app)
    {
        screen_printf("sched_add_task: unknown app '%s'\n", filename);
        return -1;
    }

    struct task_struct *task = task_slab_alloc();
    if (task == NULL)
    {
        panic("sched_add_task: too many tasks");
    }

    task->pending_signals = 0;
    pid_table[task->pid] = task;

    const void *image = app->start;
    size_t image_size = (size_t) (app->end - app->start);

    uint8_t *p = (uint8_t *) image;

    struct elf_info elf_info;

    bool success = elf_load(image, image_size, task->mem_base, &elf_info);
    if (!success)
    {
        pid_table[task->pid] = NULL;
        task_slab_free(task);
        screen_printf("Failed to load the binary\n");
        return -1;
    }

    uint32_t main_addr = elf_info.entry;

    uint32_t stack_top = align_up(task->mem_base + elf_info.size + 0x1000, 16);

    char *sp = (char *) stack_top;

    // Copy strings onto stack, remember start position
    char *strings_start = sp;
    for (int i = argc - 1; i >= 0; i--)
    {
        size_t len = 0;
        while (argv[i][len])
        { len++; }
        len++;

        sp -= len;
        for (size_t j = 0; j < len; j++)
        {
            sp[j] = argv[i][j];
        }
    }

    // Align
    sp = (char *) ((uint32_t) sp & ~3);
    uint32_t *sp32 = (uint32_t *) sp;

    // Build argv array - walk through strings again
    *(--sp32) = 0;
    char *str_ptr = (char *) stack_top;
    for (int i = argc - 1; i >= 0; i--)
    {
        size_t len = 0;
        while (argv[i][len])
        { len++; }
        len++;
        str_ptr -= len;
        *(--sp32) = (uint32_t) str_ptr;
    }
    char **new_argv = (char **) sp32;

    *(--sp32) = (uint32_t) new_argv;
    *(--sp32) = (uint32_t) argc;
    *(--sp32) = main_addr;
    *(--sp32) = 0;
    *(--sp32) = (uint32_t) task_trampoline;
    *(--sp32) = 0x202;
    *(--sp32) = 0;
    *(--sp32) = 0;
    *(--sp32) = 0;
    *(--sp32) = 0;

    task->esp = (uint32_t) sp32;
    task->eip = (uint32_t) task_trampoline;
    task->eflags = 0x202;
    task->parent = sched.current ? sched.current : task;

    run_queue_push(&sched.run_queue, task);

    return task->pid;
}

pid_t sched_fork(void)
{

}

int sched_execve(const char *pathname, char *const argv[], char *const envp[])
{
    return 0;
}

int sched_kill(pid_t pid, int sig)
{
//    if (sig <= 0 || sig > MAX_SIGNALS)
//    {
//        return -1;
//    }
//
//    if (pid >= MAX_PROCESS_CNT)
//    {
//        return -1;
//    }
//
//    struct task_struct *task = pid_table[pid];
//    if (task == NULL)
//    {
//        return -1;
//    }
//
//    task->pending_signals |= (1u << (sig - 1));
    return 0;
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

pid_t sched_getpid(void)
{
    return sched.current->pid;
}

void sched_yield(void)
{

    // if(!interrupts_are_enabled())
    // {
    //     screen_println("sched_yield; interrupts not enabled.");
    // }

    if (sched.run_queue.len == 0)
    {
        // screen_println("sched_yield; no other task.");
        return;
    }


//    screen_println("sched_yield to other task.");


    struct task_struct *prev = sched.current;
    run_queue_push(&sched.run_queue, prev);

    sched.current = run_queue_poll(&sched.run_queue);
    struct task_struct *next = sched.current;

    task_context_switch(prev, next);
}