#ifndef TASK_SLAB_H
#define TASK_SLAB_H

#include "sched.h"

void task_slab_init(void);

struct task_struct* task_slab_alloc(void);

void task_slab_free(struct task_struct *task);

#endif /* TASK_SLAB_H */
