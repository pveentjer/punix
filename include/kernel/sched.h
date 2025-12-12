#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include "constants.h"
#include "files.h"

typedef int pid_t;
typedef uint32_t sigset_t;

/* forward declaration to avoid circular include */
struct tty;

struct task
{
    pid_t pid;           // 0

    uint32_t eip;        // 4
    uint32_t esp;        // 8
    uint32_t ebp;        // 12
    uint32_t eflags;     // 16

    uint16_t ss;         // 20: stack segment selector
    uint16_t _pad;       // 22: padding for alignment

    struct task *next;   // 24 (after padding)
    struct task *parent; // 28

    uint32_t mem_base;         // 32

    char name[MAX_FILENAME_LEN]; // 36+

    struct files files;

    sigset_t pending_signals;

    struct tty *ctty;
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
    struct task_table task_table;
    struct run_queue run_queue;
    struct task *current;
};

extern struct scheduler sched;

/* Task table functions */
struct task *task_table_find_task_by_pid(
        const struct task_table *task_table,
        const pid_t pid);

void task_table_init(
        struct task_table *task_table);

struct task *task_table_alloc(
        struct task_table *task_table);

void task_table_free(
        struct task_table *task_table,
        struct task *task);

struct task *sched_current(void);

struct task *sched_find_by_pid(
        pid_t pid);

void sched_init(void);

pid_t sched_add_task(
        const char *filename,
        int argc,
        char **argv,
        int tty_id);

pid_t sched_fork(void);

int sched_kill(
        pid_t pid,
        int sig);

int sched_execve(
        const char *pathname,
        char *const argv[],
        char *const envp[]);

__attribute__((noreturn))
void sched_start(void);

void sched_yield(void);

pid_t sched_getpid(void);

void sched_exit(int status);

/* asm functions */
void task_start(
        struct task *t);

int task_context_switch(
        struct task *current,
                struct task *next);

#endif // SCHED_H