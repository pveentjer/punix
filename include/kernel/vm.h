#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

/*
 * Architecture-neutral address space descriptors.
 * No paging internals exposed.
 * Implementation-specific state is carried via `impl`.
 */

/* Process address space */
struct vm_space {
    uintptr_t base_va;   /* base virtual address */
    size_t    size;      /* size of addressable region */
    void     *impl;      /* implementation-specific state */
};

/* Kernel address space */
struct vm_kernel_space {
    uintptr_t base_va;
    void     *impl;      /* implementation-specific state */
};

/* ------------------------------------------------------------
 * Initialization
 * ------------------------------------------------------------ */

/* Initialize kernel address space */
void vm_init(void);

/* Access kernel address space */
struct vm_kernel_space *vm_kernel(void);

/* Create a new process address space */
struct vm_space *vm_create(uint32_t base_pa, size_t size);

#endif /* VM_H */
