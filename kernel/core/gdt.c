// kernel/gdt.c
#include <stdint.h>
#include "../include/kernel/gdt.h"
#include "../include/kernel/kutils.h"   // for panic(), if you have it

#define GDT_MAX 256

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  gran;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_MAX];
static struct gdt_ptr   gdt_desc;

/* ------------------------------------------------------------
 * Helper to fill one GDT entry
 * ------------------------------------------------------------ */
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t gran)
{
    gdt[idx].limit_low = limit & 0xFFFF;
    gdt[idx].base_low  = base & 0xFFFF;
    gdt[idx].base_mid  = (base >> 16) & 0xFF;
    gdt[idx].access    = access;
    gdt[idx].gran      = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[idx].base_high = (base >> 24) & 0xFF;
}

/* ------------------------------------------------------------
 * Initialize kernel GDT and load it (lgdt) from C
 * ------------------------------------------------------------ */
void gdt_init(void)
{
    // 0: null descriptor
    gdt_set_entry(0, 0, 0, 0, 0);

    // 1: kernel code segment
    // base = 0, limit = 4 GB, executable, ring 0, present, 32-bit, 4K gran
    gdt_set_entry(1, 0, 0xFFFFF, 0x9A, 0xCF);

    // 2: kernel data/stack segment
    gdt_set_entry(2, 0, 0xFFFFF, 0x92, 0xCF);

    // Clear rest: will be used later for per-process segments (SS, etc)
    for (int i = 3; i < GDT_MAX; i++) {
        gdt_set_entry(i, 0, 0, 0, 0);
    }

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base  = (uint32_t)&gdt;

    // ---- lgdt from C (no separate asm function needed) ----
    asm volatile("lgdt (%0)" :: "r" (&gdt_desc));

    // Reload segment registers to use new GDT
    asm volatile (
            "mov $0x10, %%ax\n"   // selector for GDT entry 2 (data)
            "mov %%ax, %%ds\n"
            "mov %%ax, %%es\n"
            "mov %%ax, %%fs\n"
            "mov %%ax, %%gs\n"
            "mov %%ax, %%ss\n"
            :
            :
            : "ax"
            );
}

/* ------------------------------------------------------------
 * Allocate a per-process stack/data segment.
 * base = process mem_start, size = region size (e.g. 1 MiB)
 * Returns selector to store in task->ss
 * ------------------------------------------------------------ */
uint16_t gdt_alloc_stack_segment(uint32_t base, uint32_t size)
{
    static int next = 3;  // first free index after code/data

    if (next >= GDT_MAX) {
        panic("No free GDT entries");
    }

    uint32_t limit = size - 1;
    int idx = next++;

    // Data segment, present, ring0, read/write
    uint8_t access = 0x92;
    uint8_t gran   = 0xCF;  // 4K granularity, 32-bit

    gdt_set_entry(idx, base, limit, access, gran);

    // selector = index << 3, RPL=0
    return (uint16_t)(idx << 3);
}
