#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the sp/ss, all other cpu context is stored on the stack.
    uint32_t u_sp;
    uint32_t k_sp;
};

#endif //CPU_CTX_H
