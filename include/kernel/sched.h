#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include "constants.h"
#include "dirent.h"
#include "files.h"

typedef int pid_t;

typedef uint32_t sigset_t;

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

    uint32_t  mem_base;         // 32

    char name[MAX_FILENAME_LEN]; // 36+

    struct files files;

    sigset_t pending_signals;
};


struct run_queue
{
    struct task *first;
    struct task *last;
    size_t len;
};

struct task* sched_current(void);

void sched_init(void);

pid_t  sched_add_task(const char *filename, int argc, char **argv);

pid_t sched_fork(void);

int sched_kill(pid_t pid, int sig);

int sched_execve(const char *pathname, char *const argv[], char *const envp[]);

__attribute__((noreturn))
void sched_start(void);

void sched_yield(void);

pid_t sched_getpid(void);

void sched_exit(int status);

/* asm functions */
void task_start(struct task *t);

int sched_fill_proc_dirents(struct dirent *buf, unsigned int max_entries);

int task_context_switch(struct task *current, struct task *next);

#endif // SCHED_H