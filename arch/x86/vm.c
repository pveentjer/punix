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

/* User virtual base for process mappings */
#define USER_VA_BASE       (4u * 1024u * 1024u)  /* 4MB */

/* Static sizing (up-front) */
#define VM_MAX_PROCS       64u
#define VM_USER_BYTES      (4u * 1024u * 1024u)  /* per process */
#define VM_PT_COVERS_BYTES (4u * 1024u * 1024u)  /* 1 PT covers 4MB */

#define DIV_ROUND_UP(x, y) (((x) + (y) - 1) / (y))
#define VM_USER_PTS_PER_PROC  (DIV_ROUND_UP(VM_USER_BYTES, VM_PT_COVERS_BYTES))
#define VM_PAGING_PAGES_TOTAL (VM_MAX_PROCS * (1u + VM_USER_PTS_PER_PROC)) /* PD + PTs */

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
 * VM space (PRIVATE to vm.c)
 * ------------------------------------------------------------ */

enum vm_kind {
    VM_KERNEL = 0,
    VM_PROCESS = 1,
};

struct vm_space {
    enum vm_kind kind;

    struct page_directory *pd_va; /* VA pointer to page directory */
    uintptr_t              pd_pa; /* PA of page directory (for CR3, but not exposed) */

    union {
        struct {
            /* No per-process paging allocator anymore; PD/PT come from kernel pool */
            uint32_t reserved;
        } proc;

        struct {
            uint32_t kernel_pde_start;
        } kern;
    } u;
};

static struct vm_space kernel_vm;
static struct vm_space proc_vms[VM_MAX_PROCS];
static uint32_t proc_vm_next = 0;

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
    for (uint32_t i = 0; i < PAGE_TABLE_ENTRIES; i++) {
        pt->e[i].present = 0;
        pt->e[i].writable = 0;
        pt->e[i].user = 0;
        pt->e[i].frame = 0;
    }
}

static uintptr_t vm_paging_alloc_page_pa(void)
{
    if (vm_paging_next >= VM_PAGING_PAGES_TOTAL) {
        return 0;
    }

    uintptr_t page_va = (uintptr_t)&vm_paging_pool[vm_paging_next * PAGE_SIZE];
    vm_paging_next++;

    return va_to_pa(page_va);
}

static struct vm_space *vm_alloc_process_vm(void)
{
    if (proc_vm_next >= VM_MAX_PROCS) {
        return NULL;
    }
    return &proc_vms[proc_vm_next++];
}

/* ------------------------------------------------------------
 * VM init
 * ------------------------------------------------------------ */

void vm_init(void)
{
    kernel_vm.kind = VM_KERNEL;
    kernel_vm.pd_va = kernel_pd();
    kernel_vm.pd_pa = va_to_pa((uintptr_t)kernel_vm.pd_va);

    kernel_vm.u.kern.kernel_pde_start = (uint32_t)(kernel_va_base() >> 22);

    /* Unmap premain identity-mapped physical region from the kernel VM */
    uintptr_t premain_pa_start = (uintptr_t)&__premain_pa_start;
    uintptr_t premain_pa_end   = (uintptr_t)&__premain_pa_end;

    uintptr_t premain_va_start = premain_pa_start; /* identity mapped */
    uintptr_t premain_va_end   = premain_pa_end;

    uintptr_t premain_va_page_start = PAGE_ALIGN_DOWN(premain_va_start);
    uintptr_t premain_va_page_end   = PAGE_ALIGN_UP(premain_va_end);

    vm_unmap(&kernel_vm,
             premain_va_page_start,
             premain_va_page_end - premain_va_page_start);
}

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

struct vm_space *vm_kernel(void)
{
    return &kernel_vm;
}

struct vm_space *vm_create(uint32_t base_pa, size_t size)
{
    struct vm_space *vm = vm_alloc_process_vm();
    if (!vm) {
        return NULL;
    }

    vm->kind = VM_PROCESS;

    /* Allocate PD from kernel-owned paging pool */
    uintptr_t pd_pa = vm_paging_alloc_page_pa();
    if (!pd_pa) {
        return NULL;
    }

    vm->pd_pa = pd_pa;
    vm->pd_va = (struct page_directory *)pa_to_va(pd_pa);

    /* Clear PD */
    for (uint32_t i = 0; i < PAGE_DIR_ENTRIES; i++) {
        vm->pd_va->e[i].present = 0;
        vm->pd_va->e[i].writable = 0;
        vm->pd_va->e[i].user = 0;
        vm->pd_va->e[i].frame = 0;
        vm->pd_va->e[i].ps = 0;
    }

    /* Copy kernel half mappings verbatim */
    uint32_t kstart = kernel_vm.u.kern.kernel_pde_start;
    for (uint32_t i = kstart; i < PAGE_DIR_ENTRIES; i++) {
        vm->pd_va->e[i] = kernel_vm.pd_va->e[i];
    }

    /* Map provided physical memory as user memory at USER_VA_BASE */
    if (size) {
        uintptr_t user_pa_start = PAGE_ALIGN_UP((uintptr_t)base_pa);
        uintptr_t user_pa_end   = PAGE_ALIGN_DOWN((uintptr_t)base_pa + (uintptr_t)size);

        if (user_pa_end > user_pa_start) {
            vm_map(vm, (uintptr_t)USER_VA_BASE, user_pa_start, (size_t)(user_pa_end - user_pa_start));
        }
    }

    return vm;
}

void vm_map(struct vm_space *vm, uintptr_t va, uintptr_t pa, size_t size)
{
    while (size) {
        uint32_t pde_idx = PDE_INDEX(va);
        uint32_t pte_idx = PTE_INDEX(va);

        struct pde *pde = &vm->pd_va->e[pde_idx];

        /* For process spaces: allocate PT on demand if missing (from kernel pool).
           For kernel space: we assume the PT already exists. */
        if (!pde->present) {
            if (vm->kind != VM_PROCESS) {
                return;
            }

            uintptr_t pt_pa = vm_paging_alloc_page_pa();
            if (!pt_pa) {
                return;
            }

            struct page_table *new_pt = (struct page_table *)pa_to_va(pt_pa);
            pt_clear(new_pt);

            pde->frame = (uint32_t)(pt_pa >> 12);
            pde->present = 1;
            pde->writable = 1;
            pde->user = 0;
            pde->ps = 0;
        }

        struct page_table *pt = pde_to_pt(pde);
        struct pte *pte = &pt->e[pte_idx];

        /* User mapping when below kernel VA base */
        uint32_t flags = PTE_P | PTE_W;
        if (va < kernel_va_base()) {
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

void vm_unmap(struct vm_space *vm, uintptr_t va, size_t size)
{
    while (size) {
        uint32_t pde_idx = PDE_INDEX(va);
        uint32_t pte_idx = PTE_INDEX(va);

        struct pde *pde = &vm->pd_va->e[pde_idx];
        if (pde->present) {
            struct page_table *pt = pde_to_pt(pde);
            struct pte *pte = &pt->e[pte_idx];

            pte_clear(pte);
            invlpg(va);
        }

        va += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
}
