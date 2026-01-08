#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the sp/ss, all other cpu context is stored on the stack.

    // userspace stack pointer
    unsigned long u_sp;

    // kernel stack pointer
    unsigned long k_sp;
};

#endif //CPU_CTX_H
