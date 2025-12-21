#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/* ------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------ */

// GDT segment indices
#define GDT_NULL_IDX            0
#define GDT_KERNEL_CODE_IDX     1
#define GDT_KERNEL_DATA_IDX     2
#define GDT_FIRST_FREE_IDX      3

// Per-task layout: 2 entries (code + data) per task
#define GDT_ENTRIES_PER_TASK    2

// Total number of GDT entries needed
#define GDT_ENTRY_COUNT \
    (GDT_FIRST_FREE_IDX + MAX_PROCESS_CNT * GDT_ENTRIES_PER_TASK)

// Per-task index mapping
#define GDT_TASK_CODE_IDX(task_idx) (GDT_FIRST_FREE_IDX + (task_idx) * GDT_ENTRIES_PER_TASK)
#define GDT_TASK_DATA_IDX(task_idx) (GDT_FIRST_FREE_IDX + (task_idx) * GDT_ENTRIES_PER_TASK + 1)

// Segment selectors (index << 3)
#define GDT_KERNEL_CS     ((GDT_KERNEL_CODE_IDX) << 3)
#define GDT_KERNEL_DS     ((GDT_KERNEL_DATA_IDX) << 3)

// Segment access flags
#define ACCESS_CODE_RING0       0x9A    // present, ring 0, executable, readable
#define ACCESS_DATA_RING0       0x92    // present, ring 0, data, writable

// Granularity flags
#define GRAN_4K_32BIT           0xCF    // 4K blocks, 32-bit segment

// Full 4 GiB limit (in pages)
#define LIMIT_4GB               0xFFFFF

struct cpu_ctx;

void gdt_init(void);

void gdt_init_task_ctx(struct cpu_ctx *ctx, int task_idx);

#endif
