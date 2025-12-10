// sched.c
#include "../../include/kernel/sched.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/constants.h"
#include "../../include/kernel/vfs.h"

struct scheduler sched;

int task_context_switch(struct task *current, struct task *next);

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

void sched_init(void)
{
    sched.current = NULL;
    run_queue_init(&sched.run_queue);
    task_table_init(&sched.task_table);
}

struct task *sched_current(void)
{
    return sched.current;
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
    int status = entry(argc, argv);
    sched_exit(status);
}

pid_t sched_add_task(const char *filename, int argc, char **argv)
{
    const struct embedded_app *app = find_app(filename);
    if (!app)
    {
        kprintf("sched_add_task: unknown app '%s'\n", filename);
        return -1;
    }

    struct task *task = task_table_alloc(&sched.task_table);
    if (task == NULL)
    {
        panic("sched_add_task: too many tasks");
    }

    k_strcpy(task->name, filename);

    task->pending_signals = 0;

    const void *image = app->start;
    size_t image_size = (size_t) (app->end - app->start);

    uint8_t *p = (uint8_t *) image;

    struct elf_info elf_info;

    bool success = elf_load(image, image_size, task->mem_base, &elf_info);
    if (!success)
    {
        task_table_free(&sched.task_table, task);
        kprintf("Failed to load the binary\n");
        return -1;
    }

    int fd_stdin = vfs_open(&vfs, task, "/dev/stdin", O_RDONLY, 0);
    int fd_stdout = vfs_open(&vfs, task, "/dev/stdout", O_WRONLY, 0);
    int fd_stderr = vfs_open(&vfs, task, "/dev/stderr", O_WRONLY, 0);

    struct file *f_stdin = files_find_by_fd(&task->files, fd_stdin);
    struct file *f_stdout = files_find_by_fd(&task->files, fd_stdout);
    struct file *f_stderr = files_find_by_fd(&task->files, fd_stderr);

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
    task->parent = sched.current ? sched.current : task;


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

void sched_start(void)
{
    struct task dummy;
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
    if (sched.run_queue.len == 0)
    {
        return;
    }

    struct task *prev = sched.current;
    run_queue_push(&sched.run_queue, prev);

    sched.current = run_queue_poll(&sched.run_queue);
    struct task *next = sched.current;

    task_context_switch(prev, next);
}
