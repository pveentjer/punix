#include "interrupt.h"

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

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idtp;

/* ----------------------------------------------------------------------
 * Dummy ISR: used for all vectors by default.
 * For now we just immediately iret (don’t touch regs/stack).
 * -------------------------------------------------------------------- */
__attribute__((naked))
static void isr_default(void)
{
    __asm__ volatile (
        "iret"
    );
}

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

    // Initialize all entries to a safe dummy handler
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_set_gate(i, (uint32_t)isr_default, 0x08, 0x8E);
    }

    // Load IDT
    asm volatile("lidt (%0)" : : "r"(&idtp));
}

void interrupts_enable(void) {
    asm volatile("sti");
}

bool interrupts_are_enabled(void) {
    uint32_t eflags;
    asm volatile("pushfl\n\tpopl %0" : "=r"(eflags));
    return (eflags & 0x200) != 0;  // Check IF flag (bit 9)
}
