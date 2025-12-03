#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>

typedef int pid_t;

struct task_struct
{
    pid_t pid;
    uint32_t eip;        // entry function
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
    struct task_struct *next;
    struct task_struct *parent;
};

struct run_queue
{
    struct task_struct *first;
    struct task_struct *last;
    size_t len;
};


void sched_init(void);
void sched_add_task(const char *filename, int argc, char **argv);

__attribute__((noreturn)) 
void sched_start(void);

void yield(void);

/* asm functions */
void task_start(struct task_struct *t);

int  task_context_switch(struct task_struct *current, struct task_struct *next);

#endif // SCHED_H