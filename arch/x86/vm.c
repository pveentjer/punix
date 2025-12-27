#include "kernel/vm.h"
#include "kernel/console.h"
#include "kernel/config.h"
#include "kernel/constants.h"
#include "kernel/panic.h"
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

/* Static sizing (up-front) */
#define VM_PT_COVERS_BYTES (4u * 1024u * 1024u)  /* 1 PT covers 4MB */

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))
#define VM_USER_PTS_PER_PROC  (DIV_ROUND_UP(PROCESS_VA_SIZE, VM_PT_COVERS_BYTES))
#define VM_PAGING_PAGES_TOTAL (MAX_PROCESS_CNT * (1u + VM_USER_PTS_PER_PROC)) /* PD + PTs */

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
    uintptr_t              pd_pa;      /* PA of page directory (for CR3) */
    uint32_t               kernel_pde_start;
};

/* ------------------------------------------------------------
 * Globals (public structs, private impl storage)
 * ------------------------------------------------------------ */

static struct vm_kernel_space kernel_vm;
static struct vm_space        proc_vms[MAX_PROCESS_CNT];
static uint32_t               proc_vm_next = 0;

static struct vm_impl kernel_impl;
static struct vm_impl proc_impl_pool[MAX_PROCESS_CNT];
static uint32_t       proc_impl_next = 0;

/* ------------------------------------------------------------
 * Kernel-owned paging-structure pool (PD/PT pages)
 * ------------------------------------------------------------ */

static uint8_t vm_paging_pool[VM_PAGING_PAGES_TOTAL * PAGE_SIZE]
        __attribute__((aligned(PAGE_SIZE)));

static uint32_t vm_paging_next = 0;

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static inline void invlpg(uintptr_t va)
{
    __asm__ volatile("invlpg (%0)" :: "r"(va) : "memory");
}

static inline uintptr_t kernel_pa_base(void)
{
    return (uintptr_t)&__kernel_pa_start;
}

static inline uintptr_t kernel_va_base(void)
{
    return (uintptr_t)&__kernel_va_base;
}

static inline uintptr_t pa_to_va(uintptr_t pa)
{
    return pa - kernel_pa_base() + kernel_va_base();
}

static inline uintptr_t va_to_pa(uintptr_t va)
{
    return va - kernel_va_base() + kernel_pa_base();
}

static inline struct page_directory *kernel_pd(void)
{
    return (struct page_directory *)&__kernel_page_directory_va;
}

static inline struct page_table *pde_to_pt(struct pde *pde)
{
    return (struct page_table *)pa_to_va(FRAME_TO_PA(pde->frame));
}

static inline void pte_set(struct pte *p, uintptr_t pa, uint32_t flags)
{
    p->frame    = (uint32_t)(pa >> 12);
    p->present  = !!(flags & PTE_P);
    p->writable = !!(flags & PTE_W);
    p->user     = !!(flags & PTE_U);
}

static inline void pte_clear(struct pte *p)
{
    p->present = 0;
}

static void pt_clear(struct page_table *pt)
{
    for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES; i++)
    {
        pt->e[i].present  = 0;
        pt->e[i].writable = 0;
        pt->e[i].user     = 0;
        pt->e[i].frame    = 0;
    }
}

static uintptr_t vm_paging_alloc_page_pa(void)
{
    if (vm_paging_next >= VM_PAGING_PAGES_TOTAL)
    {
        return 0;
    }

    uintptr_t page_va = (uintptr_t)&vm_paging_pool[vm_paging_next * PAGE_SIZE];
    vm_paging_next++;

    return va_to_pa(page_va);
}

static struct vm_space *vm_alloc_process_vm(void)
{
    if (proc_vm_next >= MAX_PROCESS_CNT)
    {
        return NULL;
    }
    return &proc_vms[proc_vm_next++];
}

static struct vm_impl *vm_alloc_process_impl(void)
{
    if (proc_impl_next >= MAX_PROCESS_CNT)
    {
        return NULL;
    }
    return &proc_impl_pool[proc_impl_next++];
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

        /* Allocate PT on demand (from kernel pool) */
        if (!pde->present)
        {
            uintptr_t pt_pa = vm_paging_alloc_page_pa();
            if (!pt_pa)
            {
                return;
            }

            struct page_table *new_pt = (struct page_table *)pa_to_va(pt_pa);
            pt_clear(new_pt);

            pde->frame    = (uint32_t)(pt_pa >> 12);
            pde->present  = 1;
            pde->writable = 1;
            pde->user     = 0;
            pde->ps       = 0;
        }

        struct page_table *pt = pde_to_pt(pde);
        struct pte *pte = &pt->e[pte_idx];

        /* User mapping when below kernel VA base */
        uint32_t flags = PTE_P | PTE_W;
        if (va < kernel_va_base())
        {
            flags |= PTE_U;
            pde->user = 1;
        }

        pte_set(pte, pa, flags);
        invlpg(va);

        va   += PAGE_SIZE;
        pa   += PAGE_SIZE;
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

        va   += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}

/* ------------------------------------------------------------
 * VM init
 * ------------------------------------------------------------ */

void vm_init(void)
{
    /* Bind kernel vm to existing kernel paging structures */
    kernel_vm.base_va = kernel_va_base();
    kernel_vm.impl    = &kernel_impl;

    kernel_impl.pd_va = kernel_pd();
    kernel_impl.pd_pa = va_to_pa((uintptr_t)kernel_impl.pd_va);
    kernel_impl.kernel_pde_start = (uint32_t)(kernel_va_base() >> 22);

    /* Unmap premain identity-mapped physical region from the kernel VM */
    uintptr_t premain_pa_start = (uintptr_t)&__premain_pa_start;
    uintptr_t premain_pa_end   = (uintptr_t)&__premain_pa_end;

    uintptr_t premain_va_start = premain_pa_start; /* identity mapped */
    uintptr_t premain_va_end   = premain_pa_end;

    uintptr_t premain_va_page_start = PAGE_ALIGN_DOWN(premain_va_start);
    uintptr_t premain_va_page_end   = PAGE_ALIGN_UP(premain_va_end);

    vm_unmap_impl(&kernel_impl,
                  premain_va_page_start,
                  premain_va_page_end - premain_va_page_start);
}

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

struct vm_kernel_space *vm_kernel(void)
{
    return &kernel_vm;
}

/* ------------------------------------------------------------
 * VM verification
 * ------------------------------------------------------------ */

static inline uintptr_t read_eip(void)
{
    uintptr_t eip;
    __asm__ volatile("call 1f\n1: pop %0" : "=r"(eip));
    return eip;
}

static inline uintptr_t read_esp(void)
{
    uintptr_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    return esp;
}

static int vm_is_mapped(struct page_directory *pd, uintptr_t va, uint32_t *pte_out)
{
    uint32_t pdi = PDE_INDEX(va);
    uint32_t pti = PTE_INDEX(va);

    struct pde *pde = &pd->e[pdi];
    if (!pde->present)
        return 0;

    struct page_table *pt = pde_to_pt(pde);
    struct pte *pte = &pt->e[pti];

    if (pte_out)
        *pte_out = *(uint32_t *)pte;

    return pte->present;
}

static void vm_verify(struct vm_space *vm)
{
    struct vm_impl *impl = vm->impl;
    struct page_directory *pd = impl->pd_va;
    struct page_directory *kpd = kernel_impl.pd_va;

    /* 1. Kernel PDEs must match exactly */
    uint32_t kstart = kernel_impl.kernel_pde_start;

    for (uint32_t i = kstart; i < PAGE_DIR_ENTRIES; i++) {
        if (kpd->e[i].present) {
            uint32_t a = *(uint32_t *)&pd->e[i];
            uint32_t b = *(uint32_t *)&kpd->e[i];

            if (a != b) {
                kprintf("vm_verify: kernel PDE mismatch idx=%u\n", i);
                panic("vm_verify");
            }
        }
    }

    /* 2. Current instruction must be mapped */
    uintptr_t eip = read_eip();
    if (!vm_is_mapped(pd, eip, NULL))
    {
        kprintf("vm_verify: EIP not mapped: %x\n", (uint32_t)eip);
        panic("vm_verify");
    }

    /* 3. Stack must be mapped */
    uintptr_t esp = read_esp();
    if (!vm_is_mapped(pd, esp, NULL))
    {
        kprintf("vm_verify: ESP not mapped: %x\n", (uint32_t)esp);
        panic("vm_verify");
    }

    /* 4. User range must exist + be user-accessible */
    uintptr_t start = PROCESS_VA_BASE;
    uintptr_t end   = PROCESS_VA_BASE + PROCESS_VA_SIZE;

    for (uintptr_t va = start; va < end; va += PAGE_SIZE)
    {
        uint32_t pte;
        if (!vm_is_mapped(pd, va, &pte))
        {
            kprintf("vm_verify: user VA unmapped %x\n", (uint32_t)va);
            panic("vm_verify");
        }

        if (!(pte & PTE_U))
        {
            kprintf("vm_verify: user VA not user-accessible %x\n", (uint32_t)va);
            panic("vm_verify");
        }
    }

    /* 5. Kernel mappings must NOT be user accessible */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        if (!kpd->e[i].present)
            continue;

        if ((i << 22) >= kernel_va_base())
        {
            if (pd->e[i].user)
            {
                kprintf("vm_verify: kernel PDE user-accessible idx=%u\n", i);
                panic("vm_verify");
            }
        }
    }
}


struct vm_space *vm_create(uint32_t base_pa, size_t size)
{
    struct vm_space *vm = vm_alloc_process_vm();
    if (!vm)
    {
        return NULL;
    }

    struct vm_impl *impl = vm_alloc_process_impl();
    if (!impl)
    {
        return NULL;
    }

    vm->base_va = (uintptr_t)PROCESS_VA_BASE;
    vm->size    = size;
    vm->impl    = impl;

    /* Allocate PD from kernel-owned paging pool */
    uintptr_t pd_pa = vm_paging_alloc_page_pa();
    if (!pd_pa)
    {
        return NULL;
    }

    impl->pd_pa = pd_pa;
    impl->pd_va = (struct page_directory *)pa_to_va(pd_pa);

    /* Clear PD */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        impl->pd_va->e[i].present  = 0;
        impl->pd_va->e[i].writable = 0;
        impl->pd_va->e[i].user     = 0;
        impl->pd_va->e[i].frame    = 0;
        impl->pd_va->e[i].ps       = 0;
    }

    /* Copy kernel half mappings verbatim */
    impl->kernel_pde_start = kernel_impl.kernel_pde_start;
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++)
    {
        if (kernel_impl.pd_va->e[i].present)
            impl->pd_va->e[i] = kernel_impl.pd_va->e[i];
    }

    /* Map provided physical memory as user memory at PROCESS_VA_BASE */
    if (size)
    {
        uintptr_t pa_start = PAGE_ALIGN_UP((uintptr_t)base_pa);
        uintptr_t pa_end   = PAGE_ALIGN_DOWN((uintptr_t)base_pa + (uintptr_t)size);

        if (pa_end > pa_start)
        {
            vm_map_impl(impl,
                        (uintptr_t)PROCESS_VA_BASE,
                        pa_start,
                        (size_t)(pa_end - pa_start));
        }
    }

    vm_verify(vm);

    return vm;
}



/* ------------------------------------------------------------
 * VM activation
 * ------------------------------------------------------------ */

void vm_activate(struct vm_space *vm)
{
    struct vm_impl *impl = (struct vm_impl *)vm->impl;
//
//    kprintf("vm_activate ESP = 0x%08x\n", read_esp());
//
//
//    kprintf("vm_activate:\n");
//    kprintf("  vm=%p\n", vm);
//    kprintf("  impl=%p\n", impl);
//    kprintf("  pd_pa=0x%08x\n", (uint32_t)impl->pd_pa);

    /* --- Validate stack is mapped in new PD --- */
    uintptr_t esp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));

    uint32_t pde_idx = PDE_INDEX(esp);
    uint32_t pte_idx = PTE_INDEX(esp);

    struct pde *pd = impl->pd_va->e;








    if (!pd[pde_idx].present)
    {
        kprintf("FATAL: stack PDE not present (esp=0x%08x)\n", esp);
        while (1);
    }

    struct page_table *pt = pde_to_pt(&pd[pde_idx]);

    if (!pt->e[pte_idx].present)
    {
        kprintf("FATAL: stack PTE not present (esp=0x%08x)\n", esp);
        while (1);
    }

    /* --- Disable interrupts during CR3 switch --- */
    __asm__ volatile("cli");

    /* --- Flush global mappings by clearing PGE --- */
    uint32_t cr4;
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4 & ~(1 << 7)));

    /* --- Load new page directory --- */
    __asm__ volatile("mov %0, %%cr3" :: "r"(impl->pd_pa) : "memory");

    /* --- Restore CR4 (re-enable PGE if used) --- */
    __asm__ volatile("mov %0, %%cr4" :: "r"(cr4));

    __asm__ volatile("sti");

//    kprintf("vm_activate: OK\n");
//
//    kprintf("vm_activate POST ESP = 0x%08x\n", read_esp());
}


void vm_activate_kernel(void)
{
    __asm__ volatile(
            "mov %0, %%cr3"
            :
            : "r"(kernel_impl.pd_pa)
            : "memory"
            );
}
