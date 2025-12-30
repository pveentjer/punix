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

//    struct cpu_ctx *k_cpu_ctx = &KERNEL_ENTRY->k_cpu_ctx;
//
//    k_cpu_ctx->esp = KERNEL_STACK_TOP_VA;

    bss_zero();

    console_init(&kconsole);

    kprintf("PUnix 0.001\n");
    clock_init();

    kprintf("Init Interrupt Descriptor Table.\n");
    idt_init();

    kprintf("Init VM.\n");
    mm_init();

    kprintf("Init VFS.\n");
    vfs_init(&vfs);

    kprintf("Init TTYs.\n");
    tty_system_init();

    kprintf("Init scheduler.\n");
    sched_init();

    kprintf("Enabling interrupts.\n");
    interrupts_enable();

//    kprintf("Triggering page fault...\n");
//    /* 1GB = 0x40000000 */
//    volatile uint32_t *p = (uint32_t *)0x40000000;
//    uint32_t x = *p;   // <- should page fault

    console_clear(&kconsole);
    char *argv[] = {"/sbin/init", NULL};
    sched_add_task("/sbin/init", 0, argv, 0);
    sched_schedule();
}
