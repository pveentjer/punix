#include <stdint.h>
#include "kernel/mm.h"
#include "kernel/sched.h"

int mm_brk(void *addr)
{
    struct task *task = sched_current();
    if (!task)
    {
        return -1;
    }

    uintptr_t new_brk = (uintptr_t) addr;

    if (new_brk > task->brk_limit)
    {
        return -1;
    }

    task->brk = new_brk;
    return 0;
}