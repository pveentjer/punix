// sched.c
#include "kernel/sched.h"
#include "kernel/console.h"
#include "kernel/irq.h"
#include "kernel/kutils.h"
#include "kernel/elf_loader.h"
#include "kernel/constants.h"
#include "kernel/vfs.h"

struct scheduler sched;

void ctx_init(
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
    //  kprintf("---------------------\n");

//    *(volatile uint16_t *) 0xB8F9E = 0x0F41;  // 'A' - Entry

    struct task *current = sched_current();

    // kprintf("task_trampoline: %s entry=%p, argc=%d\n", current->name, entry, argc);


//    *(volatile uint16_t *) 0xB8F9E = 0x0F42;  // 'B' - Got current

    vfs_open(&vfs, current, "/dev/stdin", O_RDONLY, 0);
//    *(volatile uint16_t *) 0xB8F9E = 0x0F43;  // 'C' - Opened stdin

    vfs_open(&vfs, current, "/dev/stdout", O_WRONLY, 0);
//    *(volatile uint16_t *) 0xB8F9E = 0x0F44;  // 'D' - Opened stdout

    vfs_open(&vfs, current, "/dev/stderr", O_WRONLY, 0);
//    *(volatile uint16_t *) 0xB8F9E = 0x0F45;  // 'E' - Opened stderr

    uint32_t u_esp = current->cpu_ctx.u_esp;
//    *(volatile uint16_t *) 0xB8F9E = 0x0F46;  // 'F' - Got u_esp



    int exit_code;

    __asm__ volatile(
//            "movw $0x0F47, 0xB8F9E\n\t"      // 'G' - In asm
            "mov %%esp, %%ebx\n\t"           // Save kernel ESP
//            "movw $0x0F48, 0xB8F9E\n\t"      // 'H' - Saved ESP
            "movl %[current], %%edi\n\t"
            "movl (%%edi), %%edi\n\t"
            "movl %%ebx, 4(%%edi)\n\t"       // Save to k_esp
//            "movw $0x0F49, 0xB8F9E\n\t"      // 'I' - Saved k_esp

            "mov %1, %%esp\n\t"               // Switch to user stack
//            "movw $0x0F4A, 0xB8F9E\n\t"      // 'J' - Switched stack
            "pushl %3\n\t"                    // Push argv
//            "movw $0x0F4B, 0xB8F9E\n\t"      // 'K' - Pushed argv
            "pushl %2\n\t"                    // Push argc
//            "movw $0x0F4C, 0xB8F9E\n\t"      // 'L' - Pushed argc
            "call *%4\n\t"                    // Call entry
//            "movw $0x0F4D, 0xB8F9E\n\t"      // 'M' - Returned
            "addl $8, %%esp\n\t"
            "mov %%ebx, %%esp\n\t"
            "mov %%eax, %0\n\t"
            : "=r"(exit_code)
            : "r"(u_esp), "r"(argc), "r"(argv), "r"(entry), [current] "m"(sched.current)
    : "ebx", "edi", "eax", "memory"
    );

//    *(volatile uint16_t *) 0xB8F9E = 0x0F4E;  // 'N' - After asm
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
        uintptr_t environ_va = task->vm_space->base_va + (uintptr_t) environ_off;
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
//    kprintf("new task %s\n", filename);

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

    task->cpu_ctx.k_esp = (uint32_t) (task->kstack + KERNEL_STACK_SIZE);
    task->cpu_ctx.u_esp = PROCESS_STACK_TOP;

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
            break;
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
            break;
        k_memcpy(envp_buf + envp_offset, envp[envc], len);
        envp_copy[envc] = envp_buf + envp_offset;
        envp_offset += len;
        envc++;
    }
    envp_copy[envc] = NULL;

    struct task *parent = sched_current();
    vm_activate(task->vm_space);

    uintptr_t base_va = task->vm_space->base_va;

    k_strcpy(task->name, filename_buf);

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

    if (elf_load(image, image_size, (uint32_t) base_va, &elf_info) < 0)
    {
        kprintf("task_new: Failed to load the binary %s\n", filename_buf);
        task_table_free(&sched.task_table, task);
        vm_activate_kernel();
        return NULL;
    }

    uint32_t main_addr = elf_info.entry_va;
    uint32_t environ_off = elf_info.environ_off;

    uintptr_t program_end = (uintptr_t) elf_info.max_offset;
    task->brk = (uint32_t) align_up(program_end, 16);
    task->brk_limit = task->brk + PROCESS_HEAP_SIZE;

    if ((uintptr_t) task->brk > task->brk_limit)
    {
        kprintf("task_new: not enough space %s\n", filename_buf);
        task_table_free(&sched.task_table, task);
        vm_activate_kernel();
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

//    kprintf("%s main_addr=%p\n", task->name, main_addr);

    ctx_init(&task->cpu_ctx, main_addr, argc_out, heap_argv, heap_envp);

    // DEBUG: Verify the stack was built correctly for swapper only
    if (k_strcmp(filename_buf, "/sbin/swapper") == 0)
    {
        uint32_t *kstack = (uint32_t *) task->cpu_ctx.k_esp;
        kprintf("After ctx_init for %s: k_esp=0x%08x, [5]=0x%08x (should be task_trampoline=0x%p)\n",
                task->name, task->cpu_ctx.k_esp, kstack[5], (void *) task_trampoline);
    }

    if (parent == NULL)
    {
        vm_activate_kernel();
    }
    else
    {
        vm_activate(parent->vm_space);
    }


//    kprintf("task_new pid:%d u_esp:0x%08x k_esp:0x%08x\n", task->pid, task->cpu_ctx.u_esp,task->cpu_ctx.k_esp);

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

struct vm_impl
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

    if (k_strcmp(next->name, "/bin/sh") == 0)
    {
        static uint32_t last_sh_kesp = 0;
        if (last_sh_kesp != 0 && next->cpu_ctx.k_esp != last_sh_kesp)
        {
            kprintf("ERROR: sh k_esp changed from 0x%08x to 0x%08x!\n",
                    last_sh_kesp, next->cpu_ctx.k_esp);
        }
        last_sh_kesp = next->cpu_ctx.k_esp;
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

    *(volatile uint16_t *) 0xB8F9E = 0x0F20;  // ' ' - Clear before ctx_switch

    kprintf("sched_schedule %s\n", next->name);

    // Show sh's kernel stack state
    if (k_strcmp(next->name, "/bin/sh") == 0)
    {
        kprintf("sh resuming: k_esp=0x%08x\n", next->cpu_ctx.k_esp);
        uint32_t *kstack = (uint32_t*)next->cpu_ctx.k_esp;
        kprintf("  Stack: [0]=0x%08x [4]=0x%08x [5]=0x%08x\n",
                kstack[0], kstack[4], kstack[5]);
    }

    ctx_switch(prev_cpu_ctx, next_cpu_ctx, next->vm_space);

    if (prev && k_strcmp(prev->name, "/bin/sh") == 0)
    {
        uint32_t *kstack = (uint32_t*)prev->cpu_ctx.k_esp;
        kprintf("sh after blocking: k_esp=0x%08x, [4]=0x%08x (EFLAGS)\n",
                prev->cpu_ctx.k_esp, kstack[4]);
        if (kstack[4] != 0x202 && kstack[4] != 0x200 && kstack[4] != 0x206)
        {
            kprintf("ERROR: sh's saved EFLAGS corrupted!\n");
        }
    }
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


