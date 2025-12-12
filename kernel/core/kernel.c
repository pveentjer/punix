#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../../include/kernel/console.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/gdt.h"
#include "../../include/kernel/tty.h"
#include "../../include/kernel/vfs.h"


/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
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
