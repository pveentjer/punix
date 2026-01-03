// sched.c
#include "errno.h"
#include "kernel/sched.h"
#include "kernel/console.h"
#include "kernel/irq.h"
#include "kernel/kutils.h"
#include "kernel/elf_loader.h"
#include "kernel/constants.h"
#include "kernel/vfs.h"

struct scheduler sched;

void ctx_setup_trampoline(
        struct cpu_ctx *cpu_ctx,
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

    // Close all files
    for (int fd = 0; fd < RLIMIT_NOFILE; fd++)
    {
        struct file *file = current->files.slots[fd].file;
        if (file)
        {
            vfs_close(current, fd);
        }
    }

    current->exit_status = status;
    current->state = TASK_ZOMBIE;
    wakeup(&current->signal.wait_exit);

    if (current->parent != current)
    {
        wakeup(&current->parent->signal.wait_child);
    }

    sched.current = NULL;

    sched_schedule();
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


    size_t len = sizeof(char *) * (argc + 1);
    task->brk += len;

    /* Copy each string */
    for (int i = 0; i < argc; i++)
    {
        const char *src = argv[i];
        size_t len = k_strlen(src) + 1;  /* include '\0' */
        char *dst = (char *) task->brk;

        k_memcpy(dst, src, len);

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
                            uint32_t environ_off)
{
    int envc = 0;
    while (envp && envp[envc] != NULL)
    {
        envc++;
    }

    /* Reserve space for envp pointer array */
    char **task_heap_envp = (char **) task->brk;
    task->brk += sizeof(char *) * (envc + 1);

    /* Copy each string */
    for (int i = 0; i < envc; i++)
    {
        size_t len = k_strlen(envp[i]) + 1; /* include '\0' */
        char *dst = (char *) task->brk;
        k_memcpy(dst, envp[i], len);
        task_heap_envp[i] = dst;
        task->brk += len;
    }

    task_heap_envp[envc] = NULL;

    /* Initialize program's global 'environ' if present */
    if (environ_off != 0)
    {
        struct vma *vma = mm_find_vma_by_type(task->mm, VMA_TYPE_PROCESS);
        uintptr_t environ_va = vma->base_va + (uintptr_t) environ_off;
        char ***environ_ptr = (char ***) environ_va;
        *environ_ptr = task_heap_envp;
    }

    return task_heap_envp;
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

    const struct embedded_bin *bin = find_bin(filename);
    if (!bin)
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

    task->cpu_ctx.k_sp = (unsigned long) (task->kstack + KERNEL_STACK_SIZE);
    task->cpu_ctx.u_sp = PROCESS_STACK_TOP;

    // Copy filename to stack buffer BEFORE switching PDs
    char filename_buf[MAX_FILENAME_LEN];
    k_strcpy(filename_buf, filename);

    //todo:ugly; but we'll rewrite when doing fork/execve
    // Copy argv/envp to stack buffers BEFORE switching PDs
    char argv_buf[1024];
    char envp_buf[1024];
    char *argv_copy[32];
    char *envp_copy[32];

    // Copy argv
    size_t argv_offset = 0;
    int argc = 0;
    while (argv && argv[argc] && argc < 31)
    {
        size_t len = k_strlen(argv[argc]) + 1;
        if (argv_offset + len > sizeof(argv_buf))
        {
            break;
        }
        k_memcpy(argv_buf + argv_offset, argv[argc], len);
        argv_copy[argc] = argv_buf + argv_offset;
        argv_offset += len;
        argc++;
    }
    argv_copy[argc] = NULL;

    // Copy envp
    size_t envp_offset = 0;
    int envc = 0;
    while (envp && envp[envc] && envc < 31)
    {
        size_t len = k_strlen(envp[envc]) + 1;
        if (envp_offset + len > sizeof(envp_buf))
        {
            break;
        }
        k_memcpy(envp_buf + envp_offset, envp[envc], len);
        envp_copy[envc] = envp_buf + envp_offset;
        envp_offset += len;
        envc++;
    }
    envp_copy[envc] = NULL;

    struct task *parent = sched_current();
    mm_activate(task->mm);

    uintptr_t base_va = mm_find_vma_by_type(task->mm, VMA_TYPE_PROCESS)->base_va;

    k_strcpy(task->name, filename_buf);

    task->ctxt = 0;
    task->sys_call_cnt = 0;
    task->exit_status = 0;
    task->signal.pending = 0;
    task->state = TASK_QUEUED;
    task->children = NULL;
    if (parent == NULL)
    {
        task->parent = task;
        task->next_sibling = NULL;
    }
    else
    {
        task->parent = parent;
        task->next_sibling = parent->children;
        parent->children = task;
    }

    wait_queue_init(&task->signal.wait_exit);
    wait_queue_init(&task->signal.wait_child);

    task_init_tty(task, tty_id);
    task_init_cwd(task);

    /* Load ELF */
    const void *image = bin->start;
    size_t image_size = (size_t) (bin->end - bin->start);

    struct elf_info elf_info;

    if (elf_load(image, image_size, (uint32_t) base_va, &elf_info) < 0)
    {
        kprintf("task_new: Failed to load the binary %s\n", filename_buf);
        task_table_free(&sched.task_table, task);
        mm_activate(mm_kernel());
        return NULL;
    }

    uint32_t main_addr = elf_info.entry_va;
    uint32_t environ_off = elf_info.environ_off;

    uintptr_t program_end = (uintptr_t) elf_info.max_offset;
    task->brk = (uintptr_t) align_up(program_end, 16);
    task->brk_limit = task->brk + PROCESS_HEAP_SIZE;

    if ((uintptr_t) task->brk > task->brk_limit)
    {
        kprintf("task_new: not enough space %s\n", filename_buf);
        task_table_free(&sched.task_table, task);
        mm_activate(mm_kernel());
        return NULL;
    }

    int argc_out = 0;
    char **heap_argv = task_init_args(task, argv_copy, &argc_out);
    char **heap_envp = task_init_env(task, envp_copy, environ_off);

    if (elf_info.curbrk_off != 0)
    {
        uintptr_t curbrk_va = base_va + (uintptr_t) elf_info.curbrk_off;
        char **curbrk_ptr = (char **) curbrk_va;
        *curbrk_ptr = (char *) task->brk;
    }

    ctx_setup_trampoline(&task->cpu_ctx, main_addr, argc_out, heap_argv, heap_envp);

    if (parent == NULL)
    {
        mm_activate(mm_kernel());
    }
    else
    {
        mm_activate(parent->mm);
    }
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

    task->signal.pending |= (1u << (sig - 1));
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

struct mm_impl
{
    void *pd_va;
    uintptr_t pd_pa;
    uint32_t kernel_pde_start;
};

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

    struct cpu_ctx dummy_cpu_ctx;
    struct cpu_ctx *prev_cpu_ctx;
    struct cpu_ctx *next_cpu_ctx = &next->cpu_ctx;
    if (prev == NULL)
    {
        prev_cpu_ctx = &dummy_cpu_ctx;
    }
    else
    {
        prev->ctxt++;
        prev_cpu_ctx = &prev->cpu_ctx;

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

    sched.ctxt++;

    ctx_switch(prev_cpu_ctx, next_cpu_ctx, next->mm);
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

static bool task_is_zombie(void *arg)
{
    struct task *task = (struct task *) arg;
    return task->state == TASK_ZOMBIE;
}

static struct task *find_child_by_pid(struct task *parent, pid_t pid)
{
    for (struct task *c = parent->children; c; c = c->next_sibling)
    {
        if (c->pid == pid)
        {
            return c;
        }
    }
    return NULL;
}

static struct task *find_zombie_child(struct task *parent)
{
    for (struct task *c = parent->children; c; c = c->next_sibling)
    {
        if (c->state == TASK_ZOMBIE)
        {
            return c;
        }
    }
    return NULL;
}

static bool has_zombie_child(void *arg)
{
    struct task *parent = (struct task *) arg;
    return find_zombie_child(parent) != NULL;
}

pid_t sched_waitpid(pid_t pid, int *status, int options)
{
    struct task *current = sched_current();
    struct task *child = NULL;

    if (options & ~WNOHANG)
    {
        return -EINVAL;
    }

    if (pid > 0)
    {
        /* ========== CASE 1: Wait for specific child ========== */

        child = find_child_by_pid(current, pid);

        /* Not our child */
        if (!child)
        {
            return -ECHILD;
        }

        /* Check if already zombie */
        if (child->state != TASK_ZOMBIE)
        {
            /* Child exists but not zombie yet */
            if (options & WNOHANG)
            {
                return 0;
            }

            /* Wait for this specific child to exit */
            wait_event(&child->signal.wait_exit, task_is_zombie, child, WAIT_INTERRUPTIBLE);

            /* After waking, check if it's a zombie */
            if (child->state != TASK_ZOMBIE)
            {
                return -EINTR;
            }
        }
    }
    else if (pid == -1)
    {
        /* ========== CASE 2: Wait for any child ========== */

        child = find_zombie_child(current);

        if (!child)
        {
            /* No zombie found */
            if (!current->children)
            {
                return -ECHILD;
            }

            if (options & WNOHANG)
            {
                return 0;
            }

            /* Wait for any child to exit */
            wait_event(&current->signal.wait_child, has_zombie_child, current, WAIT_INTERRUPTIBLE);

            /* Search again for zombie */
            child = find_zombie_child(current);

            if (!child)
            {
                return -EINTR;
            }
        }
    }
    else
    {
        return -EINVAL;
    }

    /* ========== Reap the zombie ========== */

    if (status)
    {
        *status = child->exit_status;
    }

    pid_t result = child->pid;

    /* Remove from children list */
    struct task **prev = &current->children;
    while (*prev)
    {
        if (*prev == child)
        {
            *prev = child->next_sibling;
            break;
        }
        prev = &(*prev)->next_sibling;
    }

    task_table_free(&sched.task_table, child);
    return result;
}

