#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>

typedef uint32_t irq_state_t;

/*
 * The reason why the irq_disable and irq_restore return or receive state, is
 * to safely deal with reentrant calls.
 */

/* Disable interrupts and return the previous interrupt state. */
static inline irq_state_t irq_disable(void);

/* Restore interrupts to a previous state returned by irq_disable(). */
static inline void irq_restore(irq_state_t state);

#endif //IRQ_H
