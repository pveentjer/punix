#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>

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


struct task_info {
    pid_t pid;
    char name[64];
};

int sched_get_tasks(struct task_info *tasks, int max);

void sched_init(void);
void sched_add_task(const char *filename, int argc, char **argv);

__attribute__((noreturn)) 
void sched_start(void);

void yield(void);

/* asm functions */
void task_start(struct task_struct *t);

int  task_context_switch(struct task_struct *current, struct task_struct *next);

#endif // SCHED_H