#ifndef CPU_CTX_H
#define CPU_CTX_H

struct cpu_ctx
{
    // we just need the sp/ss, all other cpu context is stored on the stack.
    uint32_t esp;

};

#endif //CPU_CTX_H
