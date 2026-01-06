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
            /* inherit parent's controlling terminal */
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
 * task_kernel_exec
 *
 * Create a new task from kernel context (used ONLY for init).
 * ------------------------------------------------------------ */
struct task *task_kernel_exec(const char *filename, int tty_id, char **argv, char **envp)
{
    if (tty_id >= (int) TTY_COUNT)
    {
        kprintf("task_kernel_exec: too high tty %d for binary %s\n", tty_id, filename);
        return NULL;
    }

    const struct embedded_bin *bin = find_bin(filename);
    if (!bin)
    {
        kprintf("task_kernel_exec: unknown binary '%s'\n", filename);
        return NULL;
    }

    struct task *task = task_table_alloc(&sched.task_table);
    if (!task)
    {
        kprintf("task_kernel_exec: failed to allocate task for %s\n", filename);
        return NULL;
    }

    task->cpu_ctx.k_sp = (unsigned long) (task->kstack + KERNEL_STACK_SIZE);
    task->cpu_ctx.u_sp = PROCESS_STACK_TOP;

    /* Activate task's address space to set it up */
    mm_activate(task->mm);

    uintptr_t base_va = mm_find_vma_by_type(task->mm, VMA_TYPE_PROCESS)->base_va;

    k_strcpy(task->name, filename);

    task->ctxt = 0;
    task->sys_call_cnt = 0;
    task->exit_status = 0;
    task->signal.pending = 0;
    task->state = TASK_QUEUED;
    task->children = NULL;
    task->parent = task;  /* Init is its own parent */
    task->next_sibling = NULL;

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
        kprintf("task_kernel_exec: Failed to load the binary %s\n", filename);
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
        kprintf("task_kernel_exec: not enough space %s\n", filename);
        task_table_free(&sched.task_table, task);
        mm_activate(mm_kernel());
        return NULL;
    }

    int argc_out = 0;
    char **heap_argv = task_init_args(task, argv, &argc_out);
    char **heap_envp = task_init_env(task, envp, environ_off);

    if (elf_info.curbrk_off != 0)
    {
        uintptr_t curbrk_va = base_va + (uintptr_t) elf_info.curbrk_off;
        char **curbrk_ptr = (char **) curbrk_va;
        *curbrk_ptr = (char *) task->brk;
    }

    struct trampoline trampoline = {
            .main_addr = main_addr,
            .argc = argc_out,
            .heap_argv = heap_argv,
            .heap_envp = heap_envp,
    };
    ctx_setup_trampoline(&task->cpu_ctx, &trampoline);

    /* Switch back to kernel address space */
    mm_activate(mm_kernel());

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

/* ------------------------------------------------------------
 * sched_kernel_exec
 *
 * Create and schedule a new task from kernel context.
 * Used ONLY during boot for init process.
 * ------------------------------------------------------------ */
pid_t sched_kernel_exec(const char *filename, int tty_id, char **argv, char **envp)
{
    struct task *task = task_kernel_exec(filename, tty_id, argv, envp);

    if (!task)
    {
        return -1;
    }

    sched_enqueue(task);
    return task->pid;
}

/* ------------------------------------------------------------
 * sched_fork
 *
 * Duplicate the current process.
 * Child returns 0, parent returns child PID.
 * ------------------------------------------------------------ */
pid_t sched_fork(void)
{
    struct task *parent = sched_current();
    if (!parent)
    {
        return -ESRCH;
    }

    struct task *child = task_table_alloc(&sched.task_table);
    if (!child)
    {
        return -ENOMEM;
    }

//    kprintf("fork:");

    /* Copy basic info */
    k_strcpy(child->name, parent->name);

    /* Parent-child relationship */
    child->parent = parent;
    child->next_sibling = parent->children;
    parent->children = child;

    /* Initialize child's kernel stack pointer to top of stack */
    child->cpu_ctx.k_sp = (unsigned long)(child->kstack + KERNEL_STACK_SIZE);

    /* Child returns to same user stack location */
    child->cpu_ctx.u_sp = parent->cpu_ctx.u_sp;

    /* Setup child's kernel stack to return via sys_return */
    ctx_setup_fork_return(&child->cpu_ctx);

//    kprintf("2");

    /* Copy heap boundaries */
    child->brk = parent->brk;
    child->brk_limit = parent->brk_limit;

    /* TODO: Fix file descriptor sharing/reference counting
    for (int fd = 0; fd < RLIMIT_NOFILE; fd++)
    {
        child->files.slots[fd] = parent->files.slots[fd];
        if (child->files.slots[fd].file)
        {
            child->files.slots[fd].file->ref_count++;
        }
    }
    */

    /* Copy cwd and TTY */
    k_strcpy(child->cwd, parent->cwd);
    child->ctty = parent->ctty;

//    kprintf("3");

    /* Initialize signals */
    child->signal.pending = 0;
    wait_queue_init(&child->signal.wait_exit);
    wait_queue_init(&child->signal.wait_child);

    /* Initialize counters */
    child->ctxt = 0;
    child->sys_call_cnt = 0;
    child->exit_status = 0;

//    kprintf("4");

    /* Copy user address space */
    struct vma *parent_vma = mm_find_vma_by_type(parent->mm, VMA_TYPE_PROCESS);
    if (parent_vma)
    {
        struct vma *child_vma = mm_find_vma_by_type(child->mm, VMA_TYPE_PROCESS);
        if (child_vma)
        {
            mm_copy_vma(child->mm, child_vma,
                        parent->mm, parent_vma,
                        parent_vma->length);
        }
    }

//    kprintf("5");

    child->state = TASK_QUEUED;
    sched_enqueue(child);

//    kprintf("6");

    return child->pid;
}

/* ------------------------------------------------------------
 * sched_execve
 *
 * Replace current process with a new program.
 * Returns -errno on failure, never returns on success.
 * ------------------------------------------------------------ */
int sched_execve(const char *pathname, char *const argv[], char *const envp[])
{
//    kprintf("sched_execve %s\n",pathname);

    struct task *current = sched_current();
    if (!current)
    {
        return -ESRCH;
    }

    /* Find the binary */
    const struct embedded_bin *bin = find_bin(pathname);
    if (!bin)
    {
        return -ENOENT;
    }

    /* TODO: Implement close-on-exec (O_CLOEXEC)
    for (int fd = 0; fd < RLIMIT_NOFILE; fd++)
    {
        struct file *file = current->files.slots[fd].file;
        if (file && (current->files.slots[fd].flags & FD_CLOEXEC))
        {
            vfs_close(current, fd);
        }
    }
    */

//    kprintf("sched_execve 1 pathname %s\n",pathname);


    /* Get process VMA */
    struct vma *vma = mm_find_vma_by_type(current->mm, VMA_TYPE_PROCESS);
    if (!vma)
    {
        return -ENOMEM;
    }

    /* Zero out user memory */
    uintptr_t base_va = vma->base_va;


    // is the execve running on the kernel stack?
    // it should be, you get here using a sys call

    // bug: zo zeroing out the process memory, causes the path name to become empty string
    // this smell like we are not in the kernel stack but in the user stack

    // also the user stack we don't want null btw; because it would fuck up the call stack
//    k_memset((void *)base_va, 0, vma->length);

    k_strcpy(current->name, pathname);

    /* Load ELF */
    const void *image = bin->start;
    size_t image_size = (size_t)(bin->end - bin->start);

    struct elf_info elf_info;
    if (elf_load(image, image_size, (uint32_t)base_va, &elf_info) < 0)
    {
        /* Failed to load - process is now broken, must exit */
        sched_exit(-1);
    }

    uint32_t main_addr = elf_info.entry_va;
    uint32_t environ_off = elf_info.environ_off;

    /* Reset heap */
    uintptr_t program_end = (uintptr_t)elf_info.max_offset;
    current->brk = (uintptr_t)align_up(program_end, 16);
    current->brk_limit = current->brk + PROCESS_HEAP_SIZE;

    if (current->brk > current->brk_limit)
    {
        sched_exit(-1);
    }

    /* Setup args and environment */
    int argc_out = 0;
    char **heap_argv = task_init_args(current, (char **)argv, &argc_out);
    char **heap_envp = task_init_env(current, (char **)envp, environ_off);

    /* Initialize curbrk if present */
    if (elf_info.curbrk_off != 0)
    {
        uintptr_t curbrk_va = base_va + (uintptr_t)elf_info.curbrk_off;
        char **curbrk_ptr = (char **)curbrk_va;
        *curbrk_ptr = (char *)current->brk;
    }

    /* Reset user stack pointer */
    current->cpu_ctx.u_sp = PROCESS_STACK_TOP;

    /* Setup trampoline to jump to new program */
    struct trampoline trampoline = {
        .main_addr = main_addr,
        .argc = argc_out,
        .heap_argv = heap_argv,
        .heap_envp = heap_envp,
    };
    ctx_setup_trampoline(&current->cpu_ctx, &trampoline);

    /* Reset signal state */
    current->signal.pending = 0;

    // can you return from execve??
    return 0;
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

//    kprintf("sched_schedule:");

    if (--countdown == 0)
    {
        kprintf("sched_schedule: ctxt=%u\n", sched.ctxt);
        countdown = 1000;
    }

//    kprintf("1");

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
//        kprintf("2");

        prev_cpu_ctx = &dummy_cpu_ctx;
    }
    else
    {
//        kprintf("3");

        prev->ctxt++;

//        kprintf("4");

        prev_cpu_ctx = &prev->cpu_ctx;

//        kprintf("5");


        if (next == sched.swapper)
        {
//            kprintf("6");


            if (prev->state == TASK_RUNNING)
            {
//                kprintf("7");

                return;
            }
        }

        if (prev->state != TASK_INTERRUPTIBLE && prev->state != TASK_UNINTERRUPTIBLE)
        {
//            kprintf("8");

            prev->state = TASK_QUEUED;

            if (prev != sched.swapper)
            {
//                kprintf("9");


                run_queue_push(&sched.run_queue, prev);
            }
        }
    }

//    kprintf("4");

    next->state = TASK_RUNNING;
    sched.current = next;

    sched.ctxt++;

//    kprintf("ctx_switch %s pid=%d\n", next->name, next->pid);

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

    sched.swapper = task_kernel_exec("/sbin/swapper", 0, argv, envp);

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