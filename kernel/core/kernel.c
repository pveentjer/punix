#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../../include/kernel/vga.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/gdt.h"
#include "../../include/kernel/keyboard.h"

/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    screen_clear();
    screen_printf("PUnix 0.001\n");

    screen_printf("Init Global Descriptor Table.\n");
    gdt_init();

    screen_printf("Init Interrupt Descriptor Table.\n");
    idt_init();
    
    screen_printf("Enabling interrupts.\n");
    interrupts_enable();
    
    screen_printf("Init keyboard.\n");
    keyboard_init();

    screen_printf("Init scheduler.\n");
    sched_init();
    
    char *argv[] = {"/bin/sh", NULL};
    sched_add_task("/bin/sh", 1, argv);
    
    sched_start();
}
