#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the stack pointer, all cpu context is stored in the stack.
    uint32_t esp;

    // These only need to be set once and will never change again.
    uint16_t code_gdt_idx;
    uint16_t data_gdt_idx;
};

#endif //CPU_CTX_H
