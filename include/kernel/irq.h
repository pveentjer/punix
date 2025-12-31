#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include <stdint.h>
#include <stdbool.h>

typedef unsigned long irq_state_t;

/* IRQ state helpers */
irq_state_t irq_disable(void);
void irq_restore(irq_state_t state);

/* IRQ handler registration */
void irq_register_handler(uint8_t vector, void (*handler)(void));

/* IDT management */
void idt_set_gate(uint8_t num, uintptr_t handler, uint16_t selector, uint8_t flags);
void idt_init(void);

/* Interrupt helpers */
void interrupts_enable(void);
bool interrupts_are_enabled(void);

#endif