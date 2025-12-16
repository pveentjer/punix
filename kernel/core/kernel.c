#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../../include/kernel/console.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/gdt.h"
#include "../../include/kernel/tty.h"
#include "../../include/kernel/vfs.h"


extern uint8_t __bss_start;
extern uint8_t __bss_end;

// The kernel is loaded as is. But the bss (global variables) section currently contains
// garbage and needs to be zeroed before it can be used.
static void bss_zero(void)
{
    for (uint8_t *p = &__bss_start; p < &__bss_end; p++)
    {
        *p = 0;
    }
}

/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    bss_zero();

    console_init(&kconsole);

    kprintf("PUnix 0.001\n");

    kprintf("Init Global Descriptor Table.\n");
    gdt_init();

    kprintf("Init Interrupt Descriptor Table.\n");
    idt_init();

    kprintf("Enabling interrupts.\n");
    interrupts_enable();

    kprintf("Init VFS.\n");
    vfs_init(&vfs);

    kprintf("Init TTYs.\n");
    tty_system_init();

    kprintf("Init scheduler.\n");
    sched_init();

    console_clear(&kconsole);
    char *argv[] = {"/sbin/init", NULL};
    sched_add_task("/sbin/init", 1, argv, 0);

    sched_schedule();
}
