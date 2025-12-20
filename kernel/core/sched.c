// sched.c
#include "kernel/sched.h"
#include "kernel/console.h"
#include "kernel/interrupt.h"
#include "kernel/kutils.h"
#include "kernel/elf_loader.h"
#include "kernel/constants.h"
#include "kernel/vfs.h"

struct scheduler sched;

int ctx_switch(struct cpu_ctx *current, struct cpu_ctx *next);

void ctx_init(
        struct cpu_ctx *cpu_ctx,
        uint32_t stack_top,
        uint32_t main_addr,
        int argc,
        char **heap_argv,
        char **heap_envp);


void task_init_cwd(struct task *task);

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

struct task *sched_find_by_pid(pid_t pid)
{
    return task_table_find_task_by_pid(&sched.task_table, pid);
}

void sched_enqueue(struct task *task)
{
    run_queue_push(&sched.run_queue, task);
}

void sched_exit(int status)
{
    struct task *current = sched.current;
    if (current == NULL)
    {
        panic("sched_exit:exit failed because there is no current task.\n");
    }

    if (current != sched.swapper)
    {
        task_table_free(&sched.task_table, current);
    }

    wakeup(&current->wait_exit);

    sched.current = NULL;
    sched_schedule();
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


/* ------------------------------------------------------------
 * task_init_args
 *
 * Copies argv[] into the process heap starting at task->brk.
 * ------------------------------------------------------------ */
static char **task_init_args(struct task *task, char **argv, int *argc_out)
{
    int argc = 0;
    while (argv && argv[argc] != NULL)
    {
        argc++;
    }

    /* Reserve space for argv pointer array */
    char **heap_argv = (char **) task->brk;
    task->brk += sizeof(char *) * (argc + 1);

    /* Copy each string */
    for (int i = 0; i < argc; i++)
    {
        size_t len = k_strlen(argv[i]) + 1;  // include '\0'
        char *dst = (char *) task->brk;
        k_memcpy(dst, argv[i], len);
        heap_argv[i] = dst;
        task->brk += len;
    }

    heap_argv[argc] = NULL;

    if (argc_out)
    {
        *argc_out = argc;
    }

    return heap_argv;
}

/* ------------------------------------------------------------
 * task_init_env
 *
 * Copies envp[] into the process heap starting at task->brk and
 * sets the ELF 'environ' symbol if present.
 * ------------------------------------------------------------ */
static char **task_init_env(struct task *task,
                            char **envp,
                            uint32_t environ_addr)
{
    int envc = 0;
    while (envp && envp[envc] != NULL)
    {
        envc++;
    }

    /* Reserve space for envp pointer array */
    char **heap_envp = (char **) task->brk;
    task->brk += sizeof(char *) * (envc + 1);

    /* Copy each string */
    for (int i = 0; i < envc; i++)
    {
        size_t len = k_strlen(envp[i]) + 1;  // include '\0'
        char *dst = (char *) task->brk;
        k_memcpy(dst, envp[i], len);
        heap_envp[i] = dst;
        task->brk += len;
    }

    heap_envp[envc] = NULL;

    /* Initialize program's global 'environ' if present */
    if (environ_addr != 0)
    {
        uint32_t absoluteEnvironAddr = task->mem_start + environ_addr;
        char ***environPtr = (char ***) absoluteEnvironAddr;
        *environPtr = heap_envp;
    }

    return heap_envp;
}

/* ------------------------------------------------------------
 * task_new
 * ------------------------------------------------------------ */
struct task *task_new(const char *filename, int tty_id, char **argv, char **envp)
{
    if (tty_id >= (int) TTY_COUNT)
    {
        kprintf("task_new: too high tty %d for binary %s\n", tty_id, filename);
        return NULL;
    }

    const struct embedded_app *app = find_app(filename);
    if (!app)
    {
        kprintf("task_new: unknown binary '%s'\n", filename);
        return NULL;
    }

    struct task *task = task_table_alloc(&sched.task_table);
    if (!task)
    {
        kprintf("task_new: failed to allocate task for %s\n", filename);
        return NULL;
    }

    k_strcpy(task->name, filename);
    task->ctxt = 0;
    task->pending_signals = 0;
    task->state = TASK_QUEUED;
    task->parent = sched.current ? sched.current : task;
    task_init_tty(task, tty_id);
    task_init_cwd(task);

    /* Load ELF */
    const void *image = app->start;
    size_t image_size = (size_t) (app->end - app->start);

    struct elf_info elf_info;
    if (elf_load(image, image_size, task->mem_start, &elf_info) < 0)
    {
        task_table_free(&sched.task_table, task);
        kprintf("task_new: Failed to load the binary %s\n", filename);
        return NULL;
    }

    uint32_t main_addr = elf_info.entry;
    uint32_t environ_addr = elf_info.environ_addr;

    uint32_t region_end = task->mem_start + PROCESS_SIZE;
    uint32_t stackTop = align_down(region_end, 16);

    uint32_t program_end = task->mem_start + elf_info.max_offset;
    task->brk = align_up(program_end, 16);

    if (task->brk >= stackTop)
    {
        kprintf("task_new: not enough space between program and stack for %s\n",
                filename);
        task_table_free(&sched.task_table, task);
        return NULL;
    }

    int argc = 0;
    char **heap_argv = task_init_args(task, argv, &argc);
    char **heap_envp = task_init_env(task, envp, environ_addr);

    if (elf_info.curbrk_addr != 0)
    {
        uint32_t absoluteCurbrkAddr = task->mem_start + elf_info.curbrk_addr;
        char **curbrk_ptr = (char **) absoluteCurbrkAddr;
        *curbrk_ptr = (char *) task->brk;
    }

    ctx_init(&task->cpu_ctx, stackTop, main_addr, argc, heap_argv, heap_envp);

    return task;
}

void task_init_cwd(struct task *task)
{
    if (task->parent == task)
    {
        k_strcpy(task->cwd, "/");
    }
    else
    {
        k_strcpy(task->cwd, sched.current->cwd);
    }
}

pid_t sched_add_task(const char *filename, int tty_id, char **argv, char **envp)
{
    struct task *task = task_new(filename, tty_id, argv, envp);
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
    if (task->state == TASK_INTERRUPTIBLE || task->state == TASK_INTERRUPTIBLE)
    {
        sched_enqueue(task);
    }

    return 0;
}

pid_t sched_getpid(void)
{
    return sched.current->pid;
}

void sched_schedule(void)
{
    static uint32_t countdown = 1000;

    if (--countdown == 0)
    {
        kprintf("sched_schedule: ctxt=%u\n", sched.ctxt);
        countdown = 1000;
    }

//    kprintf("sched_schedule %u\n", sched.ctxt);

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

        if (prev->state != TASK_INTERRUPTIBLE && prev->state != TASK_UNINTERRUPTIBLE)
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
    prev->ctxt++;
    sched.ctxt++;
    ctx_switch(&prev->cpu_ctx, &next->cpu_ctx);
}


void sched_stat(struct sched_stat *stat)
{
    if (stat == NULL)
    {
        return;
    }

    stat->ctxt = sched.ctxt;
}

void sched_init(void)
{
    sched.current = NULL;
    sched.ctxt = 0;
    run_queue_init(&sched.run_queue);

    task_table_init(&sched.task_table);

    console_clear(&kconsole);
    char *argv[] = {"/sbin/swapper", NULL};
    char *envp[] = {NULL};

    const char *filename = "/sbin/swapper";
    int tty_id = 0;

    sched.swapper = task_new(filename, tty_id, argv, envp);
    if (!sched.swapper)
    {
        panic("sched_init: failed to allocate a task for the swapper\n");
    }
}


struct waitpid_ctx
{
    pid_t pid;
};

static bool child_exited(void *obj)
{
    const struct waitpid_ctx *ctx = (const struct waitpid_ctx *) obj;
    return sched_find_by_pid(ctx->pid) == NULL;
}

pid_t sched_waitpid(pid_t pid, int *status, int options)
{
    (void) status;

    if (options & ~WNOHANG)
    {
        return -EINVAL;
    }

    if (options & WNOHANG)
    {
        return (sched_find_by_pid(pid) == NULL) ? pid : 0;
    }

    for (;;)
    {
        struct task *child = sched_find_by_pid(pid);

        if (child == NULL)
        {
            return pid;
        }

        struct waitpid_ctx ctx = {.pid = pid};

        wait_event(&child->wait_exit, child_exited, &ctx, WAIT_INTERRUPTIBLE);
    }
}


