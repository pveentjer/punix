#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../../include/kernel/vga.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/gdt.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/vfs.h"

/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    screen_clear();
    kprintf("PUnix 0.001\n");

    kprintf("Init Global Descriptor Table.\n");
    gdt_init();

    kprintf("Init Interrupt Descriptor Table.\n");
    idt_init();

    kprintf("Enabling interrupts.\n");
    interrupts_enable();

    kprintf("Init VFS.\n");
    vfs_init(&vfs);

    kprintf("Init keyboard.\n");
    keyboard_init();

    kprintf("Init scheduler.\n");
    sched_init();
    
    char *argv[] = {"/bin/sh", NULL};
    sched_add_task("/bin/sh", 1, argv);
    
    sched_start();
}
