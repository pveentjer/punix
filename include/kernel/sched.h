#ifndef SCHED_H
#define SCHED_H

#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdint.h"
#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stddef.h"

typedef int pid_t;

#define MAX_NAME_LENGTH 64

struct task_struct
{
    pid_t pid;
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
    struct task_struct *next;
    struct task_struct *parent;
    char name[MAX_NAME_LENGTH];
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