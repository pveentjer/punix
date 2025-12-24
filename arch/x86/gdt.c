// kernel/gdt.c
#include <stdint.h>
#include "include/gdt.h"
#include "kernel/kutils.h"
#include "kernel/constants.h"
#include "kernel/sched.h"   // for struct cpu_ctx



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

static struct gdt_entry gdt[GDT_ENTRY_COUNT];
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
 * Initialize kernel GDT and per-task segments
 * ------------------------------------------------------------ */
void gdt_init(void)
{
    // 0: null descriptor
    gdt_set_entry(GDT_NULL_IDX, 0, 0, 0, 0);

    // 1: kernel code segment (0–4 GiB, exec/read, ring 0)
    gdt_set_entry(GDT_KERNEL_CODE_IDX, 0, LIMIT_4GB,
                  ACCESS_CODE_RING0, GRAN_4K_32BIT);

    // 2: kernel data/stack segment (0–4 GiB, data, ring 0)
    gdt_set_entry(GDT_KERNEL_DATA_IDX, 0, LIMIT_4GB,
                  ACCESS_DATA_RING0, GRAN_4K_32BIT);

    // Per-task code/data segments:
    // For now, make them IDENTICAL to kernel segments (flat 0–4 GiB, ring 0),
    // so switching to task-specific selectors is a no-op in practice.
    for (int task_idx = 0; task_idx < MAX_PROCESS_CNT; task_idx++)
    {
        int code_idx = GDT_TASK_CODE_IDX(task_idx);
        int data_idx = GDT_TASK_DATA_IDX(task_idx);

        gdt_set_entry(code_idx, 0, LIMIT_4GB,
                      ACCESS_CODE_RING0, GRAN_4K_32BIT);

        gdt_set_entry(data_idx, 0, LIMIT_4GB,
                      ACCESS_DATA_RING0, GRAN_4K_32BIT);
    }

    gdt_desc.limit = sizeof(gdt) - 1;
    gdt_desc.base  = (uint32_t)&gdt;

    // Load new GDT
    asm volatile("lgdt (%0)" :: "r" (&gdt_desc));

    // Reload kernel segment registers
    asm volatile (
            "mov %[data_sel], %%ax\n"
            "mov %%ax, %%ds\n"
            "mov %%ax, %%es\n"
            "mov %%ax, %%fs\n"
            "mov %%ax, %%gs\n"
            "mov %%ax, %%ss\n"
            :
            : [data_sel] "i" (GDT_KERNEL_DS)
    : "ax"
    );
}

/* ------------------------------------------------------------
 * Initialize cpu_ctx GDT indices for a given task index
 * ------------------------------------------------------------ */
void gdt_init_task_ctx(struct cpu_ctx *ctx, int task_idx)
{
}
