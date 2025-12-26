#include "kernel/vm.h"
#include <stdint.h>

/*
 * These are provided by the linker script
 */
extern uint32_t __kernel_page_directory_va;
extern uint32_t __kernel_low_page_table_va;
extern uint32_t __kernel_high_page_table_va;

/* Page constants */
#define PAGE_SIZE 4096
#define PTE_P 0x001
#define PTE_W 0x002
#define PTE_U 0x004

typedef uint32_t pte_t;
typedef uint32_t pde_t;

/* One global address space */
struct vm_space {
    pde_t *pd;
};

static struct vm_space kernel_vm;

/* ------------------------------------------------------------ */
/* Helpers */

static inline void invlpg(uintptr_t va)
{
    __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
}

static inline void load_cr3(uintptr_t pa)
{
    __asm__ volatile("mov %0, %%cr3" :: "r"(pa) : "memory");
}

/* ------------------------------------------------------------ */

void vm_init(void)
{
    /* Page directory already created by linker */
    kernel_vm.pd = (pde_t *)&__kernel_page_directory_va;
}

/* ------------------------------------------------------------ */

vm_space_t *vm_kernel(void)
{
    return &kernel_vm;
}

/* ------------------------------------------------------------ */

void vm_activate(vm_space_t *vm)
{
    load_cr3((uintptr_t)vm->pd);
}

/* ------------------------------------------------------------ */
/* Map pages using existing page tables */

void vm_map(vm_space_t *vm, uintptr_t va, uintptr_t pa, size_t size)
{
    (void)vm;

    while (size) {
        uint32_t pde_idx = va >> 22;
        uint32_t pte_idx = (va >> 12) & 0x3FF;

        pde_t *pd = (pde_t *)vm->pd;

        /* Resolve page table from PDE */
        pte_t *pt = (pte_t *)(pd[pde_idx] & ~0xFFFu);

        pt[pte_idx] = (pa & ~0xFFFu) | PTE_P | PTE_W;

        invlpg(va);

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

/* ------------------------------------------------------------ */

void vm_unmap(vm_space_t *vm, uintptr_t va, size_t size)
{
    (void)vm;

    while (size) {
        uint32_t pde_idx = va >> 22;
        uint32_t pte_idx = (va >> 12) & 0x3FF;

        pde_t *pd = (pde_t *)vm->pd;
        pte_t *pt = (pte_t *)(pd[pde_idx] & ~0xFFFu);

        pt[pte_idx] = 0;
        invlpg(va);

        va += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}
