// arch/x86/exc_stub.h
#ifndef ARCH_X86_EXC_STUB_H
#define ARCH_X86_EXC_STUB_H

#include <stdint.h>

void idt_set_gate(uint8_t num, uint32_t handler,
                  uint16_t selector, uint8_t flags);

#define STR(x)  #x
#define XSTR(x) STR(x)

/* ------------------------------------------------------------
 * Exceptions WITH error code (e.g., #PF, #GP, #SS, #NP, #TS, #DF)
 * CPU pushes: error_code, eip, cs, eflags
 * After pushal (32 bytes), error_code is at [esp+32].
 *
 * C handler signature: void handler(uint32_t err)
 * ------------------------------------------------------------ */
#define MAKE_EXC_STUB_ERR(stub_name, handler_fn)                    \
    __attribute__((naked)) void stub_name(void)                     \
    {                                                               \
        __asm__ volatile(                                           \
            "pushal\n\t"                                            \
            "movl 32(%%esp), %%eax\n\t"                             \
            "pushl %%eax\n\t"                                       \
            "call " #handler_fn "\n\t"                              \
            "addl $4, %%esp\n\t"                                    \
            "popal\n\t"                                             \
            "addl $4, %%esp\n\t"                                    \
            "iret\n\t"                                              \
            : : : "memory"                                          \
        );                                                          \
    }

#endif
