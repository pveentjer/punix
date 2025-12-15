#ifndef GDT_H
#define GDT_H

#include <stdint.h>

struct cpu_ctx;

void gdt_init(void);

void gdt_init_task_ctx(struct cpu_ctx *ctx, int task_idx);

#endif
