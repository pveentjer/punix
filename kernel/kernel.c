#include <stdint.h>
#include "screen.h"

struct task_struct
{
    uint32_t pid;
    uint32_t eip;
    uint32_t esp;
    uint32_t ebp;
    uint32_t eflags;
};


/* A dummy task entry point */
void task_entry(void)
{
    for (;;)
    {
        screen_println("Task run");
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

    /* Inline initialization of the task struct */
    struct task_struct task = {
            .pid    = 0,
            .eip    = (uint32_t) task_entry,  /* entry point function */
            .esp    = 0x90000,               /* stack top */
            .ebp    = 0x90000,               /* base pointer */
            .eflags = 0x202                  /* interrupt flag set */
    };

    switch_to(&task);
}
