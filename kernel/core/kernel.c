#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "kernel/kutils.h"
#include "kernel/config.h"
#include "kernel/console.h"
#include "kernel/sched.h"
#include "kernel/syscall.h"
#include "kernel/irq.h"
#include "kernel/tty.h"
#include "kernel/vfs.h"
#include "kernel/config.h"
#include "kernel/panic.h"
#include "kernel/clock.h"
#include "kernel/vm.h"


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
    // *(volatile uint16_t*)0xB8000 = 0x1F4B;  // 'K'


    struct cpu_ctx *k_cpu_ctx = &KERNEL_ENTRY->k_cpu_ctx;

    k_cpu_ctx->esp = KERNEL_STACK_TOP;

    bss_zero();

    console_init(&kconsole);

    kprintf("PUnix 0.001\n");

    clock_init();

    kprintf("Init VM.\n");
    vm_init();

    kprintf("Init Interrupt Descriptor Table.\n");
    idt_init();

    kprintf("Init VFS.\n");
    vfs_init(&vfs);

    kprintf("Init TTYs.\n");
    tty_system_init();

    kprintf("Init scheduler.\n");
    sched_init();

    console_clear(&kconsole);
    char *argv[] = {"/sbin/init", NULL};
    sched_add_task("/sbin/init", 1, argv, 0);

//    panic("Stopping here for now");

//    kprintf("Enabling interrupts.\n");
    interrupts_enable();

    sched_schedule();
}
