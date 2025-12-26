#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

typedef struct vm_space vm_space_t;

void vm_init(void);

void vm_map(vm_space_t *vm, uintptr_t va, uintptr_t pa, size_t size);
void vm_unmap(vm_space_t *vm, uintptr_t va, size_t size);

vm_space_t *vm_kernel(void);

#endif /* VM_H */
