#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include "constants.h"
#include "files.h"
#include "cpu_ctx.h"

typedef int pid_t;
typedef uint32_t sigset_t;

struct tty;

enum sched_state
{
    TASK_POOLED,            /* task exists but is inactive; not runnable and not queued */
    TASK_QUEUED,            /* runnable and waiting in the run queue */
    TASK_RUNNING,           /* currently executing on a CPU */
    TASK_INTERRUPTIBLE,     /* sleeping; waiting for an event, may be woken by a signal */
    TASK_UNINTERRUPTIBLE    /* sleeping; waiting for an event, not woken by signals */
};




struct task
{
    struct cpu_ctx cpu_ctx;

    pid_t pid;
    struct task *next;
    struct task *parent;

    char cwd[MAX_FILENAME_LEN];

    // The start of the process memory (inclusive)
    uint32_t addr_base;

    // The end of the process memory (exclusive)
    uint32_t addr_end;

    char name[MAX_FILENAME_LEN];

    struct files files;

    sigset_t pending_signals;

    struct tty *ctty;

    enum sched_state state;

    // The number of context switches.
    uint64_t ctxt;

    uint32_t brk;

    struct wait_queue wait_exit;
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
    uint64_t ctxt;
    struct task_table task_table;
    struct run_queue run_queue;
    struct task *swapper;
    struct task *current;
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

void ctx_init(
        struct cpu_ctx *cpu_ctx,
        uint32_t stack_top,
        uint32_t main_addr,
        int argc,
        char **heap_argv,
        char **heap_envp);

int ctx_switch(struct cpu_ctx *current, struct cpu_ctx *next);

pid_t sched_waitpid(pid_t pid, int *status, int options);

#endif // SCHED_H