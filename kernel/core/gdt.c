// kernel/gdt.c
#include <stdint.h>
#include "../include/kernel/gdt.h"
#include "../include/kernel/kutils.h"   // for panic(), if available

/* ------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------ */
#define GDT_MAX                 256

// GDT segment indices
#define GDT_NULL_IDX            0
#define GDT_KERNEL_CODE_IDX     1
#define GDT_KERNEL_DATA_IDX     2
#define GDT_FIRST_FREE_IDX      3

// Segment selectors (index << 3)
#define GDT_KERNEL_CODE_SEL     ((GDT_KERNEL_CODE_IDX) << 3)
#define GDT_KERNEL_DATA_SEL     ((GDT_KERNEL_DATA_IDX) << 3)

// Segment access flags
#define ACCESS_CODE_RING0       0x9A    // present, ring 0, executable, readable
#define ACCESS_DATA_RING0       0x92    // present, ring 0, data, writable

// Granularity flags
#define GRAN_4K_32BIT           0xCF    // 4K blocks, 32-bit segment

// Full 4 GiB limit (in pages)
#define LIMIT_4GB               0xFFFFF

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
    gdt_set_entry(GDT_NULL_IDX, 0, 0, 0, 0);

    // 1: kernel code segment (0–4 GiB, exec/read, ring 0)
    gdt_set_entry(GDT_KERNEL_CODE_IDX, 0, LIMIT_4GB,
                  ACCESS_CODE_RING0, GRAN_4K_32BIT);

    // 2: kernel data/stack segment
    gdt_set_entry(GDT_KERNEL_DATA_IDX, 0, LIMIT_4GB,
                  ACCESS_DATA_RING0, GRAN_4K_32BIT);

    // Clear the rest for future use
    for (int i = GDT_FIRST_FREE_IDX; i < GDT_MAX; i++) {
        gdt_set_entry(i, 0, 0, 0, 0);
    }

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base  = (uint32_t)&gdt;

    // Load GDT
    asm volatile("lgdt (%0)" :: "r" (&gdt_desc));

    // Reload segment registers to use new GDT
    asm volatile (
            "mov %[data_sel], %%ax\n"
            "mov %%ax, %%ds\n"
            "mov %%ax, %%es\n"
            "mov %%ax, %%fs\n"
            "mov %%ax, %%gs\n"
            "mov %%ax, %%ss\n"
            :
            : [data_sel] "i" (GDT_KERNEL_DATA_SEL)
    : "ax"
    );
}

/* ------------------------------------------------------------
 * Allocate a per-process stack/data segment
 * ------------------------------------------------------------ */
uint16_t gdt_alloc_stack_segment(uint32_t base, uint32_t size)
{
    static int next = GDT_FIRST_FREE_IDX;

    if (next >= GDT_MAX) {
        panic("No free GDT entries");
    }

    uint32_t limit = size - 1;
    int idx = next++;

    gdt_set_entry(idx, base, limit, ACCESS_DATA_RING0, GRAN_4K_32BIT);

    return (uint16_t)(idx << 3);  // selector with RPL=0
}
