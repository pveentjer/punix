#include <stdint.h>
#include "screen.h"
#include "sched.h"


/* A dummy task entry point */
void task_entry(void)
{
    uint64_t i = 0;
    for (;;)
    {
        screen_print("Task run: ");
        screen_put_uint64(i++);
        screen_put_char('\n');
    }
}

static void print_bootmsg(void)
{
    screen_println("Munix 0.001");
}

/* Assembly routine that performs the jump */
extern void switch_to(struct task_struct *t);

/* Kernel entry point */
void kmain(void)
{
    screen_clear();
    print_bootmsg();

    struct task_struct task = {
            .pid    = 0,
            .eip    = (uint32_t) task_entry,  /* entry point function */
            .esp    = 0x90000,               /* stack top */
            .ebp    = 0x90000,               /* base pointer */
            .eflags = 0x202,                 /* interrupt flag set */
            .next = NULL
    };

    switch_to(&task);
}
