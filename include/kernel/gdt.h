#ifndef GDT_H
#define GDT_H

#include <stdint.h>

void gdt_init(void);

/* Allocate a per-process stack segment descriptor.
 * base = task->mem_start, size = process region size (e.g. 1 MiB).
 * Returns a selector to put in task->ss.
 */
uint16_t gdt_alloc_stack_segment(uint32_t base, uint32_t size);

#endif
