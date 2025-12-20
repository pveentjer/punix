#include <stdint.h>
#include "../../../include/kernel/irq.h"

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
