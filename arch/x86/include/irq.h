//
// Created by pveentjer on 12/23/25.
//

#ifndef ARCH_X86_IRQ_STUB_H
#define ARCH_X86_IRQ_STUB_H

#include <stdint.h>

/*
 * MAKE_IRQ_STUB(stub_name, vector_imm)
 *
 * vector_imm must be an *immediate integer literal* (e.g. 9, 32, 128),
 * so the asm becomes: pushl $9
 */
#define MAKE_IRQ_STUB(stub_name, vector_imm)                   \
    __attribute__((naked)) void stub_name(void)                \
    {                                                          \
        __asm__ volatile(                                      \
            "pushal\n\t"                                       \
            "pushl $" #vector_imm "\n\t"                       \
            "call irq_dispatch\n\t"                            \
            "addl $4, %esp\n\t"                                \
            "popal\n\t"                                        \
            "iret\n\t"                                         \
        );                                                     \
    }

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

void irq_dispatch(uint32_t vector);

#endif // ARCH_X86_IRQ_STUB_H
