#include <stdint.h>
#include <stdbool.h>
#include "kernel/irq.h"


typedef uint32_t irq_state_t;

static inline irq_state_t irq_disable(void)
{
    irq_state_t flags;
    __asm__ volatile(
            "pushfl\n\t"
            "popl %0\n\t"
            "cli"
            : "=r"(flags)
            :
            : "memory"
            );
    return flags;
}

static inline void irq_restore(irq_state_t flags)
{
    __asm__ volatile(
            "pushl %0\n\t"
            "popfl"
            :
            : "r"(flags)
            : "memory", "cc"
            );
}

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  zero;
    uint8_t  type_attr;
    uint16_t offset_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

#define IDT_ENTRIES 256

/* PIC ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/* ----------------------------------------------------------------------
 * Basic port I/O (local copy so this file is self-contained)
 * -------------------------------------------------------------------- */
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* ----------------------------------------------------------------------
 * Dummy ISR: used for all vectors by default.
 * For now we just immediately iret (no EOI, since we mask all IRQs).
 * -------------------------------------------------------------------- */
__attribute__((naked))
static void isr_default(void)
{
    __asm__ volatile (
            "iret"
            );
}

/* ----------------------------------------------------------------------
 * PIC: mask all IRQ lines by default.
 * We'll selectively unmask (e.g. IRQ1) in device init code.
 * -------------------------------------------------------------------- */
static void pic_mask_all(void)
{
    /* Master and slave PIC: mask all IRQs */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ----------------------------------------------------------------------
 * IDT helpers
 * -------------------------------------------------------------------- */

void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags) {
    idt[num].offset_low  = handler & 0xFFFF;
    idt[num].offset_high = (handler >> 16) & 0xFFFF;
    idt[num].selector    = selector;
    idt[num].zero        = 0;
    idt[num].type_attr   = flags;
}

void idt_init(void) {
    idtp.limit = sizeof(idt) - 1;
    idtp.base  = (uint32_t)&idt;

    /* Initialize all entries to a safe dummy handler */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint32_t)isr_default, 0x08, 0x8E);  // kernel code segment, interrupt gate
    }

    /* Load IDT */
    __asm__ volatile("lidt (%0)" : : "r"(&idtp));

    /* Mask all IRQs on the PIC; drivers (keyboard, timer, etc.)
       will explicitly unmask what they need. */
    pic_mask_all();
}

void interrupts_enable(void) {
    __asm__ volatile("sti");
}

bool interrupts_are_enabled(void) {
    uint32_t eflags;
    __asm__ volatile("pushfl\n\tpopl %0" : "=r"(eflags));
    return (eflags & 0x200) != 0;  // Check IF flag (bit 9)
}

