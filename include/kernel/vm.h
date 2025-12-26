#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

/* Forward declaration */
struct vm_space;

/* Initialize VM subsystem and kernel address space */
void vm_init(void);

/* Map / unmap virtual memory in a given address space */
void vm_map(struct vm_space *vm, uintptr_t va, uintptr_t pa, size_t size);
void vm_unmap(struct vm_space *vm, uintptr_t va, size_t size);

/* Return the kernel address space */
struct vm_space *vm_kernel(void);

/* Create a new process address space */
struct vm_space *vm_create(uint32_t base_pa, size_t size);

#endif /* VM_H */
