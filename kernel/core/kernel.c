#include <stdint.h>
#include <stdarg.h>
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
#include "kernel/mm.h"
#include "kernel/dev.h"

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
    clock_init();

    kprintf("Init Interrupt Descriptor Table.\n");
    idt_init();

    kprintf("Init Memory Management.\n");
    mm_init();

    dev_init();

    kprintf("Init VFS.\n");
    vfs_init(&vfs);
    vfs_mount("/", &root_fs);
    vfs_mount("/sys", &sys_fs);
    vfs_mount("/proc", &proc_fs);
    vfs_mount("/dev", &dev_fs);
    vfs_mount("/bin", &bin_fs);


    kprintf("Init TTYs.\n");
    tty_system_init();

    kprintf("Init scheduler.\n");
    sched_init();

    kprintf("Enabling interrupts.\n");
    interrupts_enable();

//    console_clear(&kconsole);
    char *argv[] = {"/sbin/init", NULL};
    sched_kernel_exec("/sbin/init", 0, argv, 0);
    sched_schedule();
}
