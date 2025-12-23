#ifndef KERNEL_IRQ_H
#define KERNEL_IRQ_H

#include <stdint.h>
#include <stdbool.h>

typedef uint32_t irq_state_t;

/* IRQ state helpers */
irq_state_t irq_disable(void);
void irq_restore(irq_state_t state);

/* IRQ handler registration */
void irq_register_handler(uint8_t vector, void (*handler)(void));

/* IDT / IRQ init */
void idt_init(void);

/* Interrupt helpers */
void interrupts_enable(void);
bool interrupts_are_enabled(void);

#endif
