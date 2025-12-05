#ifndef SCHED_H
#define SCHED_H

#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdint.h"
#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stddef.h"

typedef int pid_t;

#define MAX_NAME_LENGTH 64

struct task_struct
{
    pid_t pid;           // 0

    uint32_t eip;        // 4
    uint32_t esp;        // 8
    uint32_t ebp;        // 12
    uint32_t eflags;     // 16

    uint16_t ss;         // 20: stack segment selector
    uint16_t _pad;       // 22: padding for alignment

    struct task_struct *next;   // 24 (after padding)
    struct task_struct *parent; // 28

    uint32_t  mem_base;         // 32

    char name[MAX_NAME_LENGTH]; // 36+

    uint32_t slab_idx;
};


struct run_queue
{
    struct task_struct *first;
    struct task_struct *last;
    size_t len;
};


struct task_info
{
    pid_t pid;
    char name[64];
};

int sched_get_tasks(struct task_info *tasks, int max);

void sched_init(void);

void sched_add_task(const char *filename, int argc, char **argv);

pid_t sched_fork(void);

int sched_execve(const char *pathname, char *const argv[], char *const envp[]);

__attribute__((noreturn))
void sched_start(void);

void sched_yield(void);

pid_t sched_getpid(void);

void sched_exit(int status);

/* asm functions */
void task_start(struct task_struct *t);

int task_context_switch(struct task_struct *current, struct task_struct *next);

#endif // SCHED_H