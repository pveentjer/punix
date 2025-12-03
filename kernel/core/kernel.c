#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "../../include/kernel/vga.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include "../../include/kernel/interrupt.h"
#include "../../include/kernel/keyboard.h"


/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    screen_clear();
    screen_println("PUnix 0.001");
    
    screen_println("Initializing Interrupt Descriptor Table.");
    idt_init();
    
    screen_println("Enabling interrupts.");
    interrupts_enable();
    
    screen_println("Initializing keyboard.");
    keyboard_init();
    
    screen_println("Starting scheduler.");
    sched_init();
    
    char *argv[] = {"/sbin/init", NULL};
    sched_add_task("/sbin/init", 1, argv);
    
    sched_start();
}
