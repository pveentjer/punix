//
// Created by pveentjer on 12/23/25.
//

#ifndef ARCH_X86_IRQ_STUB_H
#define ARCH_X86_IRQ_STUB_H

#define MAKE_IRQ_STUB(stub_name, vector_const)                 \
    __attribute__((naked)) void stub_name(void)                \
    {                                                          \
        __asm__ volatile(                                      \
            "pushal\n\t"                                       \
            "pushl $" #vector_const "\n\t"                     \
            "call irq_dispatch\n\t"                            \
            "addl $4, %esp\n\t"                                \
            "popal\n\t"                                        \
            "iret\n\t"                                         \
        );                                                     \
    }

#endif //ARCH_X86_IRQ_STUB_H
