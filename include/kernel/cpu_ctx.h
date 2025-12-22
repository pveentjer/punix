#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the sp/ss, all other cpu context is stored on the stack.
    uint32_t esp;
    uint32_t ss;

    // These only need to be set once and will never change again.
    uint16_t cs;
    uint16_t ds;

    // User address-space description
    uint32_t addr_base;
    uint32_t addr_limit;
};

#endif //CPU_CTX_H
