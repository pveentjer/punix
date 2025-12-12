// sched.c
#include "../../include/kernel/sched.h"
#include "../../include/kernel/console.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/constants.h"
#include "../../include/kernel/vfs.h"

struct scheduler sched;

int task_context_switch(
        struct task *current,
        struct task *next);

void task_init_context(
        int argc,
        char *const *argv,
        struct task *task,
        uint32_t main_addr,
        uint32_t program_size);

/* ---------------- Run queue ---------------- */

void run_queue_init(struct run_queue *rq)
{
    rq->len = 0;
    rq->first = NULL;
    rq->last = NULL;
}

void run_queue_push(struct run_queue *rq, struct task *task)
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

struct task *run_queue_poll(struct run_queue *rq)
{
    if (rq->len == 0)
    {
        return NULL;
    }

    struct task *t = rq->first;
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


struct task *sched_current(void)
{
    return sched.current;
}

struct task *sched_find_by_pid(
        pid_t pid)
{

    return task_table_find_task_by_pid(&sched.task_table, pid);
}

void sched_enqueue(struct task *task)
{
    task->state = TASK_QUEUED;
    run_queue_push(&sched.run_queue, task);
}

void sched_exit(int status)
{

    struct task *current = sched.current;
    if (current == NULL)
    {
        panic("exit failed because there is no current task.\n");
    }

    // A copy of the current needs to be made because it is needed
    // for the context switch at the end, but we want to return the
    // struct to the slots before that.
    struct task copy_current = *current;

    task_table_free(&sched.task_table, current);

    struct task *next = run_queue_poll(&sched.run_queue);

    if (next == NULL)
    {
        sched.current = NULL;

        kprintf("Halted!\n");
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
    struct task *current = sched_current();

    vfs_open(&vfs, current, "/dev/stdin", O_RDONLY, 0);
    vfs_open(&vfs, current, "/dev/stdout", O_WRONLY, 0);
    vfs_open(&vfs, current, "/dev/stderr", O_WRONLY, 0);

    int exit_code = entry(argc, argv);
    sched_exit(exit_code);
}

void task_init_tty(struct task *task, int tty_id)
{
    if (tty_id >= 0)
    {
        task->ctty = tty_get((size_t) tty_id);
    }
    else
    {
        if (sched.current && sched.current->ctty)
        {
            /* inherit parent’s controlling terminal */
            task->ctty = sched.current->ctty;
        }
        else
        {
            /* first task / no parent: use current active TTY */
            task->ctty = tty_active();
        }
    }
}


void task_init_context(int argc,
                       char *const *argv,
                       struct task *task,
                       uint32_t main_addr,
                       uint32_t program_size)
{
    uint32_t stack_top = align_up(task->mem_base + program_size + 0x1000, 16);

    char *sp = (char *) stack_top;

    // Copy strings onto stack, remember start position
    char *strings_start = sp;
    for (int i = argc - 1; i >= 0; i--)
    {
        size_t len = 0;
        while (argv[i][len])
        {
            len++;
        }
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

    // Build argv slots - walk through strings again
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
}

struct task *task_new(const char *filename, int argc, char **argv, int tty_id)
{
    if (tty_id >= (int)TTY_COUNT)
    {
        kprintf("task_new: to high tty %d for binary %s\n", tty_id, filename);
        return NULL;
    }

    const struct embedded_app *app = find_app(filename);
    if (!app)
    {
        kprintf("task_new: unknown binary '%s'\n", filename);
        return NULL;
    }

    struct task *task = task_table_alloc(&sched.task_table);
    if (task == NULL)
    {
        kprintf("task_new: failed to allocate task for %s\n", filename);
        return NULL;
    }

    k_strcpy(task->name, filename);
    task->pending_signals = 0;
    task->state = TASK_QUEUED;
    task->parent = sched.current ? sched.current : task;
    task_init_tty(task, tty_id);

    const void *image = app->start;
    size_t image_size = (size_t) (app->end - app->start);

    uint8_t *p = (uint8_t *) image;

    struct elf_info elf_info;
    int res = elf_load(image, image_size, task->mem_base, &elf_info);
    if (res < 0)
    {
        task_table_free(&sched.task_table, task);
        kprintf("task_new: Failed to load the binary %s\n", filename);
        return NULL;
    }

    uint32_t main_addr = elf_info.entry;
    uint32_t program_size = elf_info.size;
    task_init_context(argc, argv, task, main_addr, program_size);
    return task;
}

pid_t sched_add_task(const char *filename, int argc, char **argv, int tty_id)
{
    struct task *task = task_new(filename, argc, argv, tty_id);
    if (!task)
    {
        return -1;
    }

    run_queue_push(&sched.run_queue, task);
    return task->pid;
}

pid_t sched_fork(void)
{
    return -ENOSYS;
}

int sched_execve(const char *pathname, char *const argv[], char *const envp[])
{
    return -ENOSYS;
}

int sched_kill(pid_t pid, int sig)
{
    if (sig <= 0 || sig > MAX_SIGNALS)
    {
        return -1;
    }

    struct task *task = task_table_find_task_by_pid(&sched.task_table, pid);
    if (task == NULL)
    {
        return -1;
    }

    task->pending_signals |= (1u << (sig - 1));
    return 0;
}


pid_t sched_getpid(void)
{
    return sched.current->pid;
}

void sched_schedule(void)
{
    struct task *prev = sched.current;
    struct task *next = run_queue_poll(&sched.run_queue);
    if (next == NULL)
    {
        next = sched.swapper;
    }

    struct task dummy;

    if (prev == NULL)
    {
        // There is no task running.
        prev = &dummy;
    }
    else
    {
        if (next == sched.swapper)
        {
            if (prev->state == TASK_RUNNING)
            {
                return;
            }
        }

        if (prev->state != TASK_BLOCKED)
        {
            prev->state = TASK_QUEUED;
            if (prev != sched.swapper)
            {
                run_queue_push(&sched.run_queue, prev);
            }
        }
    }

    next->state = TASK_RUNNING;
    sched.current = next;
    task_context_switch(prev, next);
}


void sched_init(void)
{
    sched.current = NULL;

    run_queue_init(&sched.run_queue);

    task_table_init(&sched.task_table);

    console_clear(&kconsole);
    char *argv[] = {"/sbin/swapper", NULL};

    const char *filename = "/sbin/swapper";
    int argc = 1;
    int tty_id = 0;

    sched.swapper = task_new(filename, argc, argv, tty_id);
    if (!sched.swapper)
    {
        panic("sched_init: failed to allocate a task for the swapper\n");
    }
}