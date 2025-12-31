#include "kernel/mm.h"
#include "kernel/console.h"
#include "kernel/config.h"
#include "kernel/constants.h"
#include "kernel/panic.h"
#include "kernel/irq.h"
#include "kernel/kutils.h"
#include "include/gdt.h"
#include "include/exc_stub.h"
#include <stdint.h>
#include <stddef.h>

/* ------------------------------------------------------------
 * Linker-provided symbols
 * ------------------------------------------------------------ */
extern uint32_t __kernel_page_directory_va;
extern uint32_t __kernel_low_page_table_va;
extern uint32_t __kernel_high_page_table_va;
extern uint32_t __kernel_pa_start;
extern uint32_t __kernel_va_base;

extern uint32_t __premain_pa_start;
extern uint32_t __premain_pa_end;

/* ------------------------------------------------------------
 * Paging constants
 * ------------------------------------------------------------ */

#define PAGE_SIZE          4096
#define PAGE_TABLE_ENTRIES 1024
#define PAGE_DIR_ENTRIES   1024
#define PAGE_SHIFT         12
#define PAGE_MASK          (~(PAGE_SIZE - 1))

#define PTE_P 0x001
#define PTE_W 0x002
#define PTE_U 0x004

#define PTE_INDEX(va)      (((va) >> PAGE_SHIFT) & 0x3FF)
#define PDE_INDEX(va)      ((va) >> 22)
#define FRAME_TO_PA(frame) ((uintptr_t)(frame) << PAGE_SHIFT)

/* Page alignment helpers */
#define PAGE_ALIGN_DOWN(x) ((x) & PAGE_MASK)
#define PAGE_ALIGN_UP(x)   (((x) + PAGE_SIZE - 1) & PAGE_MASK)

/* VGA text buffer */
#define VGA_TEXT_BUFFER_VA   0xB8000
#define VGA_TEXT_BUFFER_SIZE 0x1000

/* Static sizing (up-front) */
#define VM_PT_COVERS_BYTES (4u * 1024u * 1024u)  /* 1 PT covers 4MB */

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))
#define VM_USER_PTS_PER_PROC  (DIV_ROUND_UP(PROCESS_VA_SIZE, VM_PT_COVERS_BYTES))
#define VM_PAGING_PAGES_TOTAL (MAX_PROCESS_CNT * (1u + VM_USER_PTS_PER_PROC)) /* PD + PTs */

/* VMA pool sizing */
#define MAX_VMAS_PER_PROCESS 16
#define MAX_VMAS_TOTAL (MAX_PROCESS_CNT * MAX_VMAS_PER_PROCESS)

/* ------------------------------------------------------------
 * Paging structures (x86 32-bit non-PAE)
 * ------------------------------------------------------------ */

struct pte
{
    uint32_t present: 1;
    uint32_t writable: 1;
    uint32_t user: 1;
    uint32_t pwt: 1;
    uint32_t pcd: 1;
    uint32_t accessed: 1;
    uint32_t dirty: 1;
    uint32_t pat: 1;
    uint32_t global: 1;
    uint32_t ignored: 3;
    uint32_t frame: 20;
};

struct pde
{
    uint32_t present: 1;
    uint32_t writable: 1;
    uint32_t user: 1;
    uint32_t pwt: 1;
    uint32_t pcd: 1;
    uint32_t accessed: 1;
    uint32_t ignored1: 1;
    uint32_t ps: 1;
    uint32_t ignored2: 4;
    uint32_t frame: 20;
};

struct page_table
{
    struct pte e[PAGE_TABLE_ENTRIES];
};

struct page_directory
{
    struct pde e[PAGE_DIR_ENTRIES];
};

/* ------------------------------------------------------------
 * Implementation-specific VM state (PRIVATE to vm.c)
 * ------------------------------------------------------------ */

struct vm_impl
{
    struct page_directory *pd_va;      /* VA pointer to page directory */
    uintptr_t pd_pa;      /* PA of page directory (for CR3) */
    uint32_t kernel_pde_start;
};

/* ------------------------------------------------------------
 * Globals (public structs, private impl storage)
 * ------------------------------------------------------------ */

static struct mm kernel_vm;
static struct mm proc_mms[MAX_PROCESS_CNT];
static uint32_t proc_mm_next = 0;

static struct vm_impl kernel_impl;
static struct vm_impl proc_impl_pool[MAX_PROCESS_CNT];
static uint32_t proc_impl_next = 0;

/* VMA pool */
static struct vma vma_pool[MAX_VMAS_TOTAL];
static uint32_t vma_next = 0;

/* ------------------------------------------------------------
 * Kernel-owned paging-structure pool (PD/PT pages)
 * ------------------------------------------------------------ */

static uint8_t vm_paging_pool[VM_PAGING_PAGES_TOTAL * PAGE_SIZE]
        __attribute__((aligned(PAGE_SIZE)));

static uint32_t vm_paging_next = 0;

/* ------------------------------------------------------------
 * Page fault debug handler (exception #14 with error code)
 * ------------------------------------------------------------ */

__attribute__((used))
void page_fault_handler2(uint32_t err)
{
    uint32_t cr2, cr3, eip, cs, eflags;
    uint32_t *stack_ptr;

    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    __asm__ volatile("mov %%ebp, %0" : "=r"(stack_ptr));

    eip = stack_ptr[2];
    cs = stack_ptr[3];
    eflags = stack_ptr[4];

    kprintf("\n=== PAGE FAULT ===\n");
    kprintf("Address: 0x%08x ", cr2);
    kprintf("Error:   0x%08x ", err);
    kprintf("EIP:     0x%08x ", eip);
    kprintf("CS:      0x%04x ", cs);
    kprintf("EFLAGS:  0x%08x ", eflags);
    kprintf("CR3:     0x%08x\n ", cr3);

    kprintf("  Type:   %s ", (err & 0x01) ? "protection-violation" : "not-present");
    kprintf("  Access: %s ", (err & 0x02) ? "write" : "read");
    kprintf("  Mode:   %s ", (err & 0x04) ? "user-mode" : "kernel-mode");

    if (err & 0x08)
        kprintf("  Reserved bit set in page table entry\n");

    if (err & 0x10)
        kprintf("  Instruction fetch\n");

    panic("page fault");
}

__attribute__((used))
void page_fault_handler(uint32_t err)
{
    uint32_t cr2, eip;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    uint32_t *stack_ptr;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack_ptr));
    eip = stack_ptr[9];

    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (cr2 >> (28 - i*4)) & 0xF;
        char c = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        ((volatile uint16_t*)0xB8000)[i] = 0x4F00 | c;
    }

    for (int i = 0; i < 8; i++) {
        uint8_t nibble = (eip >> (28 - i*4)) & 0xF;
        char c = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        ((volatile uint16_t*)0xB8000)[i+9] = 0x4F00 | c;
    }

    while(1);
}

MAKE_EXC_STUB_ERR(isr_page_fault, page_fault_handler)

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static inline void invlpg(uintptr_t va)
{
    __asm__ volatile("invlpg (%0)"::"r"(va) : "memory");
}

static inline uintptr_t kernel_pa_base(void)
{
    return (uintptr_t) &__kernel_pa_start;
}

static inline uintptr_t kernel_va_base(void)
{
    return (uintptr_t) &__kernel_va_base;
}

static inline uintptr_t kernel_pa_to_va(uintptr_t pa)
{
    return pa - kernel_pa_base() + kernel_va_base();
}

static inline uintptr_t kernel_va_to_pa(uintptr_t va)
{
    return va - kernel_va_base() + kernel_pa_base();
}

static inline struct page_directory *kernel_pd(void)
{
    return (struct page_directory *) &__kernel_page_directory_va;
}

static inline struct page_table *pde_to_pt(struct pde *pde)
{
    return (struct page_table *) kernel_pa_to_va(FRAME_TO_PA(pde->frame));
}

static inline void pte_set(struct pte *p, uintptr_t pa, uint32_t flags)
{
    p->frame = (uint32_t) (pa >> 12);
    p->present = !!(flags & PTE_P);
    p->writable = !!(flags & PTE_W);
    p->user = !!(flags & PTE_U);
}

static inline void pte_clear(struct pte *p)
{
    p->present = 0;
}

static void pt_clear(struct page_table *pt)
{
    for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
    {
        pt->e[i].present = 0;
        pt->e[i].writable = 0;
        pt->e[i].user = 0;
        pt->e[i].frame = 0;
    }
}

static uintptr_t vm_paging_alloc_page_pa(void)
{
    if (vm_paging_next >= VM_PAGING_PAGES_TOTAL)
    {
        return 0;
    }

    uintptr_t page_va = (uintptr_t) &vm_paging_pool[vm_paging_next * PAGE_SIZE];
    vm_paging_next++;

    return kernel_va_to_pa(page_va);
}

static struct mm *mm_alloc(void)
{
    if (proc_mm_next >= MAX_PROCESS_CNT)
    {
        return NULL;
    }
    return &proc_mms[proc_mm_next++];
}

static struct vm_impl *vm_alloc_process_impl(void)
{
    if (proc_impl_next >= MAX_PROCESS_CNT)
    {
        return NULL;
    }
    return &proc_impl_pool[proc_impl_next++];
}

static struct vma *vma_alloc(void)
{
    if (vma_next >= MAX_VMAS_TOTAL)
    {
        return NULL;
    }
    return &vma_pool[vma_next++];
}

/* ------------------------------------------------------------
 * Internal mapping (PRIVATE to vm.c)
 * ------------------------------------------------------------ */

static void vm_map_impl(struct vm_impl *impl, uintptr_t va, uintptr_t pa, size_t size)
{
    while (size)
    {
        uint32_t pde_idx = PDE_INDEX(va);
        uint32_t pte_idx = PTE_INDEX(va);

        struct pde *pde = &impl->pd_va->e[pde_idx];

        if (!pde->present)
        {
            uintptr_t pt_pa = vm_paging_alloc_page_pa();
            if (!pt_pa)
            {
                return;
            }

            struct page_table *new_pt = (struct page_table *) kernel_pa_to_va(pt_pa);
            pt_clear(new_pt);

            pde->frame = (uint32_t) (pt_pa >> 12);
            pde->present = 1;
            pde->writable = 1;
            pde->user = 0;
            pde->ps = 0;
        }

        struct page_table *pt = pde_to_pt(pde);
        struct pte *pte = &pt->e[pte_idx];

        uint32_t flags = PTE_P | PTE_W;
        if (va < kernel_va_base())
        {
            flags |= PTE_U;
            pde->user = 1;
        }

        pte_set(pte, pa, flags);
        invlpg(va);

        va += PAGE_SIZE;
        pa += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

static void vm_unmap_impl(struct vm_impl *impl, uintptr_t va, size_t size)
{
    while (size)
    {
        uint32_t pde_idx = PDE_INDEX(va);
        uint32_t pte_idx = PTE_INDEX(va);

        struct pde *pde = &impl->pd_va->e[pde_idx];
        if (pde->present)
        {
            struct page_table *pt = pde_to_pt(pde);
            struct pte *pte = &pt->e[pte_idx];

            pte_clear(pte);
            invlpg(va);
        }

        va += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

static void vm_unmap_premain(void)
{
    uintptr_t premain_pa_start = (uintptr_t) &__premain_pa_start;
    uintptr_t premain_pa_end = (uintptr_t) &__premain_pa_end;

    uintptr_t premain_va_start = premain_pa_start;
    uintptr_t premain_va_end = premain_pa_end;

    uintptr_t premain_va_page_start = PAGE_ALIGN_DOWN(premain_va_start);
    uintptr_t premain_va_page_end = PAGE_ALIGN_UP(premain_va_end);

    vm_unmap_impl(&kernel_impl,
                  premain_va_page_start,
                  premain_va_page_end - premain_va_page_start);
}

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

void mm_init(void)
{
    idt_set_gate(14, (uint32_t) isr_page_fault, (uint16_t) GDT_KERNEL_CS, (uint8_t) 0x8E);

    kernel_vm.impl = &kernel_impl;
    kernel_vm.vmas = NULL;

    kernel_impl.pd_va = kernel_pd();
    kernel_impl.pd_pa = kernel_va_to_pa((uintptr_t) kernel_impl.pd_va);
    kernel_impl.kernel_pde_start = PDE_INDEX(kernel_va_base());

    vm_unmap_premain();

    mm_add_vma(&kernel_vm, VMA_TYPE_VGA, VGA_TEXT_BUFFER_VA, VGA_TEXT_BUFFER_SIZE, VMA_READ | VMA_WRITE, VGA_TEXT_BUFFER_VA);

    mm_activate(&kernel_vm);
}

struct mm *mm_kernel(void)
{
    return &kernel_vm;
}

struct mm *mm_fork_kernel(void)
{
    struct mm *mm = mm_alloc();
    if (!mm)
    {
        return NULL;
    }

    struct vm_impl *impl = vm_alloc_process_impl();
    if (!impl)
    {
        return NULL;
    }

    mm->impl = impl;
    mm->vmas = NULL;

    uintptr_t pd_pa = vm_paging_alloc_page_pa();
    if (!pd_pa)
    {
        return NULL;
    }

    impl->pd_pa = pd_pa;
    impl->pd_va = (struct page_directory *) kernel_pa_to_va(pd_pa);

    /* Clear PD */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        impl->pd_va->e[i].present = 0;
        impl->pd_va->e[i].writable = 0;
        impl->pd_va->e[i].user = 0;
        impl->pd_va->e[i].frame = 0;
        impl->pd_va->e[i].ps = 0;
    }

    impl->kernel_pde_start = kernel_impl.kernel_pde_start;

    /* Copy all present kernel PDEs */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        if (kernel_impl.pd_va->e[i].present)
            impl->pd_va->e[i] = kernel_impl.pd_va->e[i];
    }

    /* Copy kernel VMAs */
    for (struct vma *kv = kernel_vm.vmas; kv; kv = kv->next)
    {
        struct vma *v = vma_alloc();
        if (!v)
        {
            return NULL;
        }
        v->base_va = kv->base_va;
        v->base_pa = kv->base_pa;
        v->length = kv->length;
        v->flags = kv->flags;
        v->type = kv->type;
        v->next = mm->vmas;
        mm->vmas = v;
    }

    return mm;
}

struct vma *mm_add_vma(struct mm *mm, uint32_t type, uintptr_t va, size_t size, uint32_t flags, uintptr_t pa)
{
    struct vma *v = vma_alloc();
    if (!v)
    {
        return NULL;
    }

    v->base_va = va;
    v->base_pa = pa;
    v->length = size;
    v->flags = flags;
    v->type = type;
    v->next = mm->vmas;
    mm->vmas = v;

    /* Map the pages */
    struct vm_impl *impl = mm->impl;
    vm_map_impl(impl, va, pa, size);

    return v;
}

struct vma *mm_find_vma_by_type(struct mm *mm, uint32_t type)
{
    for (struct vma *v = mm->vmas; v; v = v->next)
    {
        if (v->type == type)
            return v;
    }
    return NULL;
}

void mm_activate(struct mm *mm)
{
    struct vm_impl *impl = (struct vm_impl *) mm->impl;

    uintptr_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));

    uint32_t pde_idx = PDE_INDEX(esp);
    uint32_t pte_idx = PTE_INDEX(esp);

    struct pde *pd = impl->pd_va->e;

    if (!pd[pde_idx].present)
    {
        kprintf("FATAL: stack PDE not present (esp=0x%08x)\n", (uint32_t) esp);
        while (1)
        {}
    }

    struct page_table *pt = pde_to_pt(&pd[pde_idx]);

    if (!pt->e[pte_idx].present)
    {
        kprintf("FATAL: stack PTE not present (esp=0x%08x)\n", (uint32_t) esp);
        while (1)
        {}
    }

    __asm__ volatile("cli");

    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %0, %%cr4"::"r"(cr4 & ~(1 << 7)));

    __asm__ volatile("mov %0, %%cr3"::"r"(impl->pd_pa) : "memory");

    __asm__ volatile("mov %0, %%cr4"::"r"(cr4));

    __asm__ volatile("sti");
}

bool mm_va_to_pa(const struct mm *mm, uint32_t va, uint32_t *out_pa)
{
    if (!mm || !out_pa)
    {
        return false;
    }

    struct vm_impl *impl = mm->impl;
    struct page_directory *pd = impl->pd_va;

    uint32_t pde_idx = PDE_INDEX(va);
    uint32_t pte_idx = PTE_INDEX(va);

    struct pde *pde = &pd->e[pde_idx];

    if (!pde->present)
    {
        return false;
    }

    struct page_table *pt =
            (struct page_table *) kernel_pa_to_va(FRAME_TO_PA(pde->frame));

    struct pte *pte = &pt->e[pte_idx];

    if (!pte->present)
    {
        return false;
    }

    *out_pa = FRAME_TO_PA(pte->frame) | (va & (PAGE_SIZE - 1));
    return true;
}

/* Temporary mapping addresses - reserve space for max VMA size */
#define TEMP_COPY_VA_SRC   0xFF000000   /* Source VMA mapped here */
#define TEMP_COPY_VA_DEST  0xFE000000   /* Dest VMA mapped here */

void mm_copy_vma(struct mm *dest_mm, struct vma *dest_vma,
                 const struct mm *src_mm, const struct vma *src_vma,
                 size_t length)
{
    /* Validate length fits in both VMAs */
    if (length > src_vma->length || length > dest_vma->length)
    {
        panic("mm_copy_vma: length exceeds VMA bounds");
    }

    /* Map entire source VMA to temp kernel VA */
    uintptr_t src_va = src_vma->base_va;
    for (size_t offset = 0; offset < length; offset += PAGE_SIZE)
    {
        uint32_t src_pa;
        if (!mm_va_to_pa(src_mm, src_va + offset, &src_pa))
            panic("mm_copy_vma: source VA not mapped");
        vm_map_impl(&kernel_impl, TEMP_COPY_VA_SRC + offset, src_pa, PAGE_SIZE);
    }

    /* Map entire dest VMA to temp kernel VA */
    uintptr_t dest_va = dest_vma->base_va;
    for (size_t offset = 0; offset < length; offset += PAGE_SIZE)
    {
        uint32_t dest_pa;
        if (!mm_va_to_pa(dest_mm, dest_va + offset, &dest_pa))
            panic("mm_copy_vma: dest VA not mapped");
        vm_map_impl(&kernel_impl, TEMP_COPY_VA_DEST + offset, dest_pa, PAGE_SIZE);
    }

    /* Copy entire VMA in one go */
    k_memcpy((void*)TEMP_COPY_VA_DEST, (void*)TEMP_COPY_VA_SRC, length);

    /* Unmap */
    vm_unmap_impl(&kernel_impl, TEMP_COPY_VA_SRC, length);
    vm_unmap_impl(&kernel_impl, TEMP_COPY_VA_DEST, length);
}

uint32_t vm_debug_read_pd_pa(void)
{
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}