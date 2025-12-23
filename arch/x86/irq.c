// arch/x86/irq.c
#include <stdint.h>
#include <stdbool.h>
#include "kernel/irq.h"

/* ------------------------------------------------------------
 * IRQ state helpers
 * ------------------------------------------------------------ */
typedef uint32_t irq_state_t;

inline irq_state_t irq_disable(void)
{
    irq_state_t flags;
    __asm__ volatile(
            "pushfl\n\t"
            "popl %0\n\t"
            "cli\n\t"
            : "=r"(flags)
            :
            : "memory"
            );
    return flags;
}

inline void irq_restore(irq_state_t flags)
{
    __asm__ volatile(
            "pushl %0\n\t"
            "popfl\n\t"
            :
            : "r"(flags)
            : "memory", "cc"
            );
}

/* ------------------------------------------------------------
 * IDT structures
 * ------------------------------------------------------------ */
struct idt_entry
{
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr
{
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

/* ------------------------------------------------------------
 * PIC ports
 * ------------------------------------------------------------ */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1
#define PIC_EOI      0x20

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idtp;

/* ------------------------------------------------------------
 * IRQ handler table (C handlers only)
 * ------------------------------------------------------------ */
static void (*irq_handlers[IDT_ENTRIES])(void);

/* ------------------------------------------------------------
 * Port I/O
 * ------------------------------------------------------------ */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ------------------------------------------------------------
 * IDT helper
 * ------------------------------------------------------------ */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

/* ------------------------------------------------------------
 * PIC helpers
 * ------------------------------------------------------------ */
static void pic_mask_all(void)
{
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ------------------------------------------------------------
 * IRQ dispatcher (called by arch-specific stubs)
 * ------------------------------------------------------------ */
void irq_dispatch(uint32_t vector)
{
    if (vector >= IDT_ENTRIES)
    {
        return;
    }

    void (*h)(void) = irq_handlers[vector];
    if (h)
    {
        h();
    }

    /*
     * Send EOI for hardware IRQs.
     *
     * Your current system uses the legacy (non-remapped) PIC layout:
     *   Master IRQs: vectors 0x08..0x0F
     *   Slave  IRQs: vectors 0x70..0x77
     *
     * (If you later remap PIC to 0x20..0x2F, adjust this accordingly.)
     */
    if ((vector >= 0x08 && vector <= 0x0F) ||
        (vector >= 0x70 && vector <= 0x77))
    {
        if (vector >= 0x70)
        {
            outb(PIC2_COMMAND, PIC_EOI);
        }
        outb(PIC1_COMMAND, PIC_EOI);
    }
}

/* ------------------------------------------------------------
 * IRQ registration API (register C handler only)
 * ------------------------------------------------------------ */
void irq_register_handler(uint8_t vector, void (*handler)(void))
{
    irq_handlers[vector] = handler;
}

/* ------------------------------------------------------------
 * Default ISR: just iret
 * (drivers install real stubs via idt_set_gate)
 * ------------------------------------------------------------ */
__attribute__((naked))
static void isr_default(void)
{
    __asm__ volatile("iret\n\t");
}

/* ------------------------------------------------------------
 * IDT init
 * ------------------------------------------------------------ */
void idt_init(void)
{
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    for (int i = 0; i < IDT_ENTRIES; i++)
    {
        irq_handlers[i] = 0;
        idt_set_gate((uint8_t)i, (uint32_t)isr_default, 0x08, 0x8E);
    }

    __asm__ volatile("lidt (%0)" : : "r"(&idtp));

    pic_mask_all();
}

/* ------------------------------------------------------------
 * Interrupt helpers
 * ------------------------------------------------------------ */
void interrupts_enable(void)
{
    __asm__ volatile("sti");
}

bool interrupts_are_enabled(void)
{
    uint32_t eflags;
    __asm__ volatile("pushfl\n\tpopl %0" : "=r"(eflags));
    return (eflags & 0x200u) != 0;
}
