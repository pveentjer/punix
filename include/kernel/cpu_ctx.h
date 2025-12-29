#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the sp/ss, all other cpu context is stored on the stack.
    uint32_t u_esp;
    uint32_t k_esp;
};

#endif //CPU_CTX_H
