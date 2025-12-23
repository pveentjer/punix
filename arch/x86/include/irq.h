//
// arch/x86/irq_stub.h
//

#ifndef ARCH_X86_IRQ_STUB_H
#define ARCH_X86_IRQ_STUB_H

#include <stdint.h>

/* ------------------------------------------------------------
 * PIC ports
 * ------------------------------------------------------------ */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

/* ------------------------------------------------------------
 * Arch-specific IDT helper
 * ------------------------------------------------------------ */
void idt_set_gate(uint8_t num, uint32_t handler,
                  uint16_t selector, uint8_t flags);

/* ------------------------------------------------------------
 * Stringify helpers
 * ------------------------------------------------------------ */
#define STR(x)  #x
#define XSTR(x) STR(x)

/*
 * MAKE_IRQ_STUB(stub_name, handler_fn, eoi_pic2)
 *
 * - stub_name : symbol name of the generated ISR stub
 * - handler_fn: C function to call (void handler(void))
 * - eoi_pic2  : immediate literal (0 or 1)
 *                0 => EOI to master PIC only
 *                1 => EOI to slave PIC then master
 *
 * Notes:
 *  - No segment switching yet (by design)
 *  - No irq_dispatch, no vector plumbing
 *  - Flat, explicit, predictable
 */
#define MAKE_IRQ_STUB(stub_name, handler_fn, eoi_pic2)            \
    __attribute__((naked)) void stub_name(void)                   \
    {                                                             \
        __asm__ volatile(                                         \
            "pushal\n\t"                                          \
            /* TODO: load kernel DS/ES/FS/GS here */              \
            "call " #handler_fn "\n\t"                            \
            /* TODO: restore DS/ES/FS/GS here */                  \
            "movb $" XSTR(PIC_EOI) ", %al\n\t"                    \
            ".if " XSTR(eoi_pic2) "\n\t"                          \
            "outb %al, $" XSTR(PIC2_COMMAND) "\n\t"              \
            ".endif\n\t"                                          \
            "outb %al, $" XSTR(PIC1_COMMAND) "\n\t"              \
            "popal\n\t"                                           \
            "iret\n\t"                                           \
        );                                                        \
    }

#endif /* ARCH_X86_IRQ_STUB_H */
