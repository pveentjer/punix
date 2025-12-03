#ifndef INTERRUPT_H
#define INTERRUPT_H

#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdint.h"
#include "../../../../../usr/lib/gcc/x86_64-redhat-linux/15/include/stdbool.h"

void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);
void interrupts_enable(void);
bool interrupts_are_enabled(void);

#endif // INTERRUPT_H