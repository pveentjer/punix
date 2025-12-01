// screen.h
#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>

struct task_struct
{
    uint32_t pid;
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
};

#endif // SCHED_H
