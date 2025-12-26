#include "kernel/vm.h"
#include "kernel/console.h"
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------
 * Linker-provided symbols
 * ------------------------------------------------------------ */
extern uint32_t __kernel_page_directory_va;
extern uint32_t __kernel_low_page_table_va;
extern uint32_t __kernel_high_page_table_va;
extern uint32_t __kernel_pa_start;
extern uint32_t __kernel_va_start;

extern uint32_t __premain_pa_start;
extern uint32_t __premain_pa_end;

/* ------------------------------------------------------------
 * Paging constants
 * ------------------------------------------------------------ */

#define PAGE_SIZE          4096
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIR_ENTRIES   1024

#define PTE_P 0x001
#define PTE_W 0x002
#define PTE_U 0x004

/* ------------------------------------------------------------
 * Paging structures
 * ------------------------------------------------------------ */

struct pte {
    uint32_t present  : 1;
    uint32_t writable : 1;
    uint32_t user     : 1;
    uint32_t pwt      : 1;
    uint32_t pcd      : 1;
    uint32_t accessed : 1;
    uint32_t dirty    : 1;
    uint32_t pat      : 1;
    uint32_t global   : 1;
    uint32_t ignored  : 3;
    uint32_t frame    : 20;
};

struct pde {
    uint32_t present  : 1;
    uint32_t writable : 1;
    uint32_t user     : 1;
    uint32_t pwt      : 1;
    uint32_t pcd      : 1;
    uint32_t accessed : 1;
    uint32_t ignored1 : 1;
    uint32_t ps       : 1;
    uint32_t ignored2 : 4;
    uint32_t frame    : 20;
};

struct page_table {
    struct pte e[PAGE_TABLE_ENTRIES];
};

struct page_directory {
    struct pde e[PAGE_DIR_ENTRIES];
};

/* ------------------------------------------------------------
 * VM space
 * ------------------------------------------------------------ */

struct vm_space {
    struct page_directory *pd;
};

static struct vm_space kernel_vm;

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static inline uint32_t pde_index(uintptr_t va)
{
    return va >> 22;
}

static inline uint32_t pte_index(uintptr_t va)
{
    return (va >> 12) & 0x3FF;
}

static inline uintptr_t pde_get_pt_pa(struct pde p)
{
    return (uintptr_t)(p.frame << 12);
}

static inline void pte_set(struct pte *p, uintptr_t pa, uint32_t flags)
{
    p->frame    = pa >> 12;
    p->present  = !!(flags & PTE_P);
    p->writable = !!(flags & PTE_W);
    p->user     = !!(flags & PTE_U);
}

static inline void pte_clear(struct pte *p)
{
    p->present = 0;
}

static inline void invlpg(uintptr_t va)
{
    __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
}

static inline void load_cr3(uintptr_t pa)
{
    __asm__ volatile("mov %0, %%cr3" :: "r"(pa) : "memory");
}

/* ------------------------------------------------------------
 * Kernel PD accessor
 * ------------------------------------------------------------ */

static inline struct page_directory *kernel_pd(void)
{
    return (struct page_directory *)&__kernel_page_directory_va;
}

#define PAGE_SHIFT 12

static inline uintptr_t kernel_pa_base(void)
{
    return (uintptr_t)&__kernel_pa_start;
}

static inline uintptr_t kernel_va_base(void)
{
    return (uintptr_t)&__kernel_va_start;
}

static inline uintptr_t pa_to_va(uintptr_t pa)
{
    return pa - kernel_pa_base() + kernel_va_base();
}

static inline uintptr_t va_to_pa(uintptr_t va)
{
    return va - kernel_va_base() + kernel_pa_base();
}

#define PTE_INDEX(va) (((va) >> PAGE_SHIFT) & 0x3FF)
#define FRAME_TO_PA(frame)   ((uintptr_t)(frame) << PAGE_SHIFT)
#define PDE_INDEX(va) ((va) >> 22)

/* ------------------------------------------------------------
 * VM init
 * ------------------------------------------------------------ */

void vm_init(void)
{
    kernel_vm.pd = kernel_pd();

    uintptr_t premain_pa = (uintptr_t)&__premain_pa_start;
    uintptr_t premain_va = premain_pa;

    uint32_t pde_index = PDE_INDEX(premain_va);
    struct pde *pde = &kernel_vm.pd->e[pde_index];

    uintptr_t pt_pa = FRAME_TO_PA(pde->frame);
    uintptr_t pt_va = pa_to_va(pt_pa);
    struct page_table *pt = (struct page_table *)pt_va;

    uint32_t pte_index = PTE_INDEX(premain_va);
    struct pte *pte = &pt->e[pte_index];

    pte->present = 0;
    invlpg(premain_va);
}

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

struct vm_space *vm_kernel(void)
{
    return &kernel_vm;
}

void vm_activate(struct vm_space *vm)
{
    load_cr3((uintptr_t)vm->pd);
}

void vm_map(struct vm_space *vm, uintptr_t va, uintptr_t pa, size_t size)
{
    while (size) {
        uint32_t pde = pde_index(va);
        uint32_t pte = pte_index(va);

        struct page_table *pt =
                (struct page_table *)(uintptr_t)(vm->pd->e[pde].frame << 12);

        pte_set(&pt->e[pte], pa, PTE_P | PTE_W);

        invlpg(va);

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

void vm_unmap(struct vm_space *vm, uintptr_t va, size_t size)
{
    while (size) {
        uint32_t pde = pde_index(va);
        uint32_t pte = pte_index(va);

        struct page_table *pt =
                (struct page_table *)(uintptr_t)(vm->pd->e[pde].frame << 12);

        pte_clear(&pt->e[pte]);

        invlpg(va);

        va += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}
