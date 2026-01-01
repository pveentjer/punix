#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include "constants.h"
#include "files.h"
#include "cpu_ctx.h"
#include "mm.h"

typedef int pid_t;
typedef uint32_t sigset_t;

struct tty;

enum sched_state
{
    TASK_POOLED = 1,            /* task exists but is inactive; not runnable and not queued */
    TASK_QUEUED = 2,            /* runnable and waiting in the run queue */
    TASK_RUNNING = 3,           /* currently executing on a CPU */
    TASK_INTERRUPTIBLE = 4,     /* sleeping; waiting for an event, may be woken by a signal */
    TASK_UNINTERRUPTIBLE = 5,   /* sleeping; waiting for an event, not woken by signals */
    TASK_ZOMBIE = 6,            /* The thread has died, but has not been reaped */
};

struct signal{
    sigset_t pending;
    struct wait_queue wait_child;
    struct wait_queue wait_exit;
};


struct task
{
    struct cpu_ctx cpu_ctx;

    pid_t pid;
    struct task *next;
    struct task *parent;

    struct task *children;      /* Head of my children list */
    struct task *next_sibling;  /* Next child in parent's list */


    int exit_status;

    char cwd[MAX_FILENAME_LEN];

    struct mm *mm;

    char name[MAX_FILENAME_LEN];

    struct files files;

    struct tty *ctty;

    enum sched_state state;

    // The number of context switches.
    uint64_t ctxt;

    uintptr_t brk;
    uintptr_t brk_limit;

    uint8_t kstack[KERNEL_STACK_SIZE];

    struct signal signal;
};

/* Task table - must be defined before struct scheduler */
struct task_slot
{
    struct task task;
    uint32_t generation;
};

struct task_table
{
    uint32_t free_ring[MAX_PROCESS_CNT];
    uint32_t free_head;
    uint32_t free_tail;
    struct task_slot slots[MAX_PROCESS_CNT];
};

struct run_queue
{
    struct task *first;
    struct task *last;
    size_t len;
};

struct scheduler
{
    struct task *current;
    uint64_t ctxt;
    struct task_table task_table;
    struct run_queue run_queue;
    struct task *swapper;

};

struct sched_stat
{
    uint64_t ctxt;
};

extern struct scheduler sched;

struct task *task_table_find_task_by_pid(const struct task_table *task_table, const pid_t pid);

void task_table_init(struct task_table *task_table);

struct task *task_table_alloc(struct task_table *task_table);

void task_table_free(struct task_table *task_table, struct task *task);

struct task *sched_current(void);

struct task *sched_find_by_pid(pid_t pid);

void sched_init(void);

void sched_stat(struct sched_stat *stat);

pid_t sched_add_task(const char *filename, int tty_id, char **argv, char **envp);

pid_t sched_fork(void);

int sched_kill(pid_t pid, int sig);

int sched_execve(const char *pathname, char *const argv[], char *const envp[]);

void sched_schedule(void);

void sched_enqueue(
        struct task *task);

pid_t sched_getpid(void);

void sched_exit(int status);

void ctx_setup_trampoline(
        struct cpu_ctx *cpu_ctx,
        uint32_t main_addr,
        int argc,
        char **heap_argv,
        char **heap_envp);

int ctx_switch(struct cpu_ctx *current, struct cpu_ctx *next, struct mm *mm);

pid_t sched_waitpid(pid_t pid, int *status, int options);

#endif // SCHED_H