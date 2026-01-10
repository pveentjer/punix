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
#define MM_PAGING_PAGES_TOTAL (MAX_PROCESS_CNT * (1u + VM_USER_PTS_PER_PROC)) /* PD + PTs */

/* VMA pool sizing */
#define MAX_VMAS_PER_PROCESS 16
#define MAX_VMAS_TOTAL (MAX_PROCESS_CNT * MAX_VMAS_PER_PROCESS)

/* ------------------------------------------------------------
 * Copy window (ONE page table max; 4MB hard cap)
 * ------------------------------------------------------------ */
#define COPY_WINDOW_MAX_BYTES   VM_PT_COVERS_BYTES

/* Put both src+dst in the SAME PT (same PDE), different PTEs */
#define COPY_SRC_VA   0xFF000000u
#define COPY_DST_VA   0xFF001000u

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

struct mm_impl
{
    struct page_directory *pd_va;      /* VA pointer to page directory */
    uintptr_t pd_pa;      /* PA of page directory (for CR3) */
    uint32_t kernel_pde_start;
};

/* ------------------------------------------------------------
 * Globals (public structs, private impl storage)
 * ------------------------------------------------------------ */

static struct mm kernel_mm;
static struct mm proc_mms[MAX_PROCESS_CNT];
static uint32_t proc_mm_next = 0;

static struct mm_impl kernel_impl;
static struct mm_impl mm_impl_pool[MAX_PROCESS_CNT];
static uint32_t mm_impl_next = 0;

/* VMA pool */
static struct vma vma_pool[MAX_VMAS_TOTAL];
static uint32_t vma_next = 0;

/* ------------------------------------------------------------
 * Kernel-owned paging-structure pool (PD/PT pages)
 * ------------------------------------------------------------ */

static uint8_t mm_paging_pool[MM_PAGING_PAGES_TOTAL * PAGE_SIZE]
        __attribute__((aligned(PAGE_SIZE)));

static uint32_t mm_paging_next = 0;

/* ------------------------------------------------------------
 * Permanent copy-window PT (inherited by all processes)
 * ------------------------------------------------------------ */
static struct page_table *copy_pt = NULL;

/* ------------------------------------------------------------
 * Page fault debug handler (exception #14 with error code)
 * ------------------------------------------------------------ */

__attribute__((used))
__attribute__((used))
void page_fault_handler(uint32_t err)
{
    uint32_t cr2, eip;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

    // Stack layout when handler is called:
    // [esp+0]  = return address (to stub)
    // [esp+4]  = error code (passed as parameter)
    // [esp+8]  = EIP (CPU pushed)
    // [esp+12] = CS  (CPU pushed)
    // [esp+16] = EFLAGS (CPU pushed)

    uint32_t *stack;
    __asm__ volatile("mov %%esp, %0" : "=r"(stack));

    eip = stack[2];  // EIP at esp+8
    uint32_t cs = stack[3];
    uint32_t eflags = stack[4];
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));

    /* classify based on instruction pointer: below kernel base == "user" */
    bool user_mode = (eip < (uintptr_t)&__kernel_va_base);

    kprintf("\033[1;37;41m\n=== PAGE FAULT ===\033[0m\n");
    kprintf("Address: 0x%08x ", cr2);
    kprintf("Error:   0x%08x ", err);
    kprintf("EIP:     0x%08x ", eip);
    kprintf("CS:      0x%04x ", cs);
    kprintf("EFLAGS:  0x%08x ", eflags);
    kprintf("CR3:     0x%08x\n", cr3);

    kprintf("  Type:   %s ", (err & 0x01) ? "protection-violation" : "not-present");
    kprintf("  Access: %s ", (err & 0x02) ? "write" : "read");
    kprintf("  Mode:   %s ", user_mode ? "user-mode" : "kernel-mode");

    if (err & 0x08)
        kprintf("  Reserved bit set in page table entry\n");

    if (err & 0x10)
        kprintf("  Instruction fetch\n");

    panic("page fault");
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

static uintptr_t mm_alloc_page_pa(void)
{
    if (mm_paging_next >= MM_PAGING_PAGES_TOTAL)
    {
        return 0;
    }

    uintptr_t page_va = (uintptr_t) &mm_paging_pool[mm_paging_next * PAGE_SIZE];
    mm_paging_next++;

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

static struct mm_impl *mm_alloc_impl(void)
{
    if (mm_impl_next >= MAX_PROCESS_CNT)
    {
        return NULL;
    }
    return &mm_impl_pool[mm_impl_next++];
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

static void vm_map_impl(struct mm_impl *impl, uintptr_t va, uintptr_t pa, size_t size)
{
    while (size)
    {
        uint32_t pde_idx = PDE_INDEX(va);
        uint32_t pte_idx = PTE_INDEX(va);

        struct pde *pde = &impl->pd_va->e[pde_idx];

        if (!pde->present)
        {
            uintptr_t pt_pa = mm_alloc_page_pa();
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

static void vm_unmap_impl(struct mm_impl *impl, uintptr_t va, size_t size)
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

    kernel_mm.impl = &kernel_impl;
    kernel_mm.vmas = NULL;

    kernel_impl.pd_va = kernel_pd();
    kernel_impl.pd_pa = kernel_va_to_pa((uintptr_t) kernel_impl.pd_va);
    kernel_impl.kernel_pde_start = PDE_INDEX(kernel_va_base());

    vm_unmap_premain();

    mm_add_vma(&kernel_mm, VMA_TYPE_VGA, VGA_TEXT_BUFFER_VA, VGA_TEXT_BUFFER_SIZE, VMA_READ | VMA_WRITE,
               VGA_TEXT_BUFFER_VA);

    /* ------------------------------------------------------------
     * Permanent copy window (one PT, PTEs non-present by default)
     * ------------------------------------------------------------ */
    {
        uint32_t pde_idx = PDE_INDEX(COPY_SRC_VA);

        uintptr_t pt_pa = mm_alloc_page_pa();
        if (!pt_pa)
        {
            panic("mm_init: copy PT alloc failed");
        }

        copy_pt = (struct page_table *) kernel_pa_to_va(pt_pa);
        pt_clear(copy_pt);

        struct pde *pde = &kernel_impl.pd_va->e[pde_idx];
        pde->frame = (uint32_t) (pt_pa >> 12);
        pde->present = 1;
        pde->writable = 1;
        pde->user = 0;
        pde->ps = 0;

        /* Ensure no stale TLB (mostly irrelevant this early, but deterministic) */
        invlpg(COPY_SRC_VA);
        invlpg(COPY_DST_VA);
    }

    mm_activate(&kernel_mm);
}

struct mm *mm_kernel(void)
{
    return &kernel_mm;
}

struct mm *mm_fork_kernel(void)
{
    struct mm *mm = mm_alloc();
    if (!mm)
    {
        return NULL;
    }

    struct mm_impl *impl = mm_alloc_impl();
    if (!impl)
    {
        return NULL;
    }

    mm->impl = impl;
    mm->vmas = NULL;

    uintptr_t pd_pa = mm_alloc_page_pa();
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

    /* Copy all present kernel PDEs (includes the copy-window PDE) */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        if (kernel_impl.pd_va->e[i].present)
        {
            impl->pd_va->e[i] = kernel_impl.pd_va->e[i];
        }
    }

    /* Copy kernel VMAs */
    for (struct vma *kv = kernel_mm.vmas; kv; kv = kv->next)
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
    struct mm_impl *impl = mm->impl;
    vm_map_impl(impl, va, pa, size);

    return v;
}

struct vma *mm_find_vma_by_type(struct mm *mm, uint32_t type)
{
    for (struct vma *v = mm->vmas; v; v = v->next)
    {
        if (v->type == type)
        {
            return v;
        }
    }
    return NULL;
}

void mm_activate(struct mm *mm)
{
    struct mm_impl *impl = (struct mm_impl *) mm->impl;

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

    struct mm_impl *impl = mm->impl;
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

void mm_copy_vma(struct mm *dest_mm, struct vma *dest_vma,
                 const struct mm *src_mm, const struct vma *src_vma,
                 size_t length)
{
    if (!copy_pt)
    {
        panic("mm_copy_vma: copy window not initialized");
    }

    /* Hard cap: one PT window max (4MB) */
    if (length > COPY_WINDOW_MAX_BYTES)
    {
        panic("mm_copy_vma: length exceeds 1-PT copy window");
    }

    /* Validate length fits in both VMAs */
    if (length > src_vma->length || length > dest_vma->length)
    {
        panic("mm_copy_vma: length exceeds VMA bounds");
    }

    size_t offset = 0;

    while (offset < length)
    {
        uintptr_t src_va = src_vma->base_va + offset;
        uintptr_t dst_va = dest_vma->base_va + offset;

        uintptr_t src_page_va = PAGE_ALIGN_DOWN(src_va);
        uintptr_t dst_page_va = PAGE_ALIGN_DOWN(dst_va);

        uint32_t src_pa, dst_pa;

        /* Translate to PA (source PD is assumed active, but this works regardless) */
        if (!mm_va_to_pa(src_mm, (uint32_t)src_page_va, &src_pa))
        {
            panic("mm_copy_vma: source VA not mapped");
        }

        if (!mm_va_to_pa(dest_mm, (uint32_t)dst_page_va, &dst_pa))
        {
            panic("mm_copy_vma: dest VA not mapped");
        }

        struct pte *src_pte = &copy_pt->e[PTE_INDEX(COPY_SRC_VA)];
        struct pte *dst_pte = &copy_pt->e[PTE_INDEX(COPY_DST_VA)];

        /* Arm mappings (PTEs normally non-present) */
        pte_set(src_pte, (uintptr_t)(src_pa & PAGE_MASK), PTE_P);
        pte_set(dst_pte, (uintptr_t)(dst_pa & PAGE_MASK), PTE_P | PTE_W);

        invlpg(COPY_SRC_VA);
        invlpg(COPY_DST_VA);

        size_t src_off = (size_t)(src_va - src_page_va);
        size_t dst_off = (size_t)(dst_va - dst_page_va);

        size_t chunk = PAGE_SIZE - src_off;
        size_t tmp = PAGE_SIZE - dst_off;
        if (tmp < chunk) chunk = tmp;

        size_t remain = length - offset;
        if (remain < chunk) chunk = remain;

        k_memcpy((void *)(COPY_DST_VA + dst_off),
                 (void *)(COPY_SRC_VA + src_off),
                 chunk);

        /* Disarm immediately */
        pte_clear(src_pte);
        pte_clear(dst_pte);

        invlpg(COPY_SRC_VA);
        invlpg(COPY_DST_VA);

        offset += chunk;
    }
}

uint32_t vm_debug_read_pd_pa(void)
{
    uint32_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}
