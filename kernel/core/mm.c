#include <stdint.h>
#include "../../include/kernel/mm.h"
#include "../../include/kernel/sched.h"

int mm_brk(void *addr)
{
    struct task *task = sched_current();
    if (!task)
    {
        return -1;
    }

    uint32_t new_brk = (uint32_t) addr;

    if (new_brk >= task->mem_end)
    {
        return -1;
    }

    task->brk = new_brk;
    return 0;
}