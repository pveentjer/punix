#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct vm_impl;

/* VMA types */
#define VMA_TYPE_KERNEL  0
#define VMA_TYPE_VGA     1
#define VMA_TYPE_PROCESS 2

/* Virtual memory area - a mapped region */
struct vma {
    uintptr_t base_va;
    uintptr_t base_pa;
    size_t length;
    uint32_t flags;
    uint32_t type;
    struct vma *next;
};

/* VMA flags */
#define VMA_READ  0x1
#define VMA_WRITE 0x2
#define VMA_EXEC  0x4
#define VMA_USER  0x8

/* Memory management structure */
struct mm {
    void *impl;        /* Page directory (architecture-specific) */
    struct vma *vmas;  /* List of mapped regions */
};

/* ------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------ */

/* Initialize VM subsystem */
void mm_init(void);

/* Get kernel mm */
struct mm *mm_kernel(void);

/* ------------------------------------------------------------
 * MM Operations
 * ------------------------------------------------------------ */

/* Create new mm by forking kernel mm (copies kernel VMAs and mappings) */
struct mm *mm_fork_kernel(void);

/* Add a VMA region and map physical pages */
struct vma *mm_add_vma(struct mm *mm, uint32_t type, uintptr_t va, size_t size, uint32_t flags, uintptr_t pa);

/* Find first VMA with given type */
struct vma *mm_find_vma_by_type(struct mm *mm, uint32_t type);

/* Switch to this address space */
void mm_activate(struct mm *mm);

/* Translate virtual to physical address */
bool mm_va_to_pa(const struct mm *mm, uint32_t va, uint32_t *out_pa);

int mm_brk(void *addr);

#endif /* VM_H */