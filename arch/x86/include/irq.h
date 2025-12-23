//
// arch/x86/irq_stub.h
//

#ifndef ARCH_X86_IRQ_STUB_H
#define ARCH_X86_IRQ_STUB_H

#include <stdint.h>
#include "gdt.h"

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
 * Segment handling:
 *  - Save DS/ES/FS/GS
 *  - Load kernel DS into DS/ES/FS/GS
 *  - Call handler
 *  - Restore DS/ES/FS/GS
 */
#define MAKE_IRQ_STUB(stub_name, handler_fn, eoi_pic2)              \
    __attribute__((naked)) void stub_name(void)                     \
    {                                                               \
        __asm__ volatile(                                           \
            "pushal\n\t"                                            \
            /* save segment regs */                                 \
            "pushl %ds\n\t"                                         \
            "pushl %es\n\t"                                         \
            "pushl %fs\n\t"                                         \
            "pushl %gs\n\t"                                         \
            /* load kernel data segment into all data seg regs */   \
            "movw $" XSTR(GDT_KERNEL_DS) ", %ax\n\t"                \
            "movw %ax, %ds\n\t"                                     \
            "movw %ax, %es\n\t"                                     \
            "movw %ax, %fs\n\t"                                     \
            "movw %ax, %gs\n\t"                                     \
            /* call the real handler */                             \
            "call " #handler_fn "\n\t"                              \
            /* restore segment regs */                              \
            "popl %gs\n\t"                                          \
            "popl %fs\n\t"                                          \
            "popl %es\n\t"                                          \
            "popl %ds\n\t"                                          \
            /* EOI */                                               \
            "movb $" XSTR(PIC_EOI) ", %al\n\t"                      \
            ".if " XSTR(eoi_pic2) "\n\t"                            \
            "outb %al, $" XSTR(PIC2_COMMAND) "\n\t"                 \
            ".endif\n\t"                                            \
            "outb %al, $" XSTR(PIC1_COMMAND) "\n\t"                 \
            "popal\n\t"                                             \
            "iret\n\t"                                              \
        );                                                          \
    }

#endif /* ARCH_X86_IRQ_STUB_H */
