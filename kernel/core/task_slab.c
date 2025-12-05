#include "../../include/kernel/task_slab.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/constants.h"


static uint32_t free_list[MAX_PROCESS_CNT];
static uint32_t free_index;
static struct task_struct slab[MAX_PROCESS_CNT];

void task_slab_init(void)
{
    free_index = 0;
    for (int k = 0; k < MAX_PROCESS_CNT; k++)
    {
        struct task_struct *task = &slab[k];
        task->slab_idx = k;
        task->pid = k;
        task->mem_base = PROCESS_BASE + k * PROCESS_SIZE;
        free_list[k] = k;
    }
}

struct task_struct *task_slab_alloc(void)
{
    if (free_index == MAX_PROCESS_CNT)
    {
        return NULL;
    }

    int task_idx = free_list[free_index];
    struct task_struct *task = &slab[task_idx];

    free_index++;

    return task;
}

void task_slab_free(struct task_struct *task_struct)
{
    if (free_index == 0)
    {
        panic("task_slab: too many frees.");
    }

    free_index--;
    free_list[free_index] = task_struct->slab_idx;
}

