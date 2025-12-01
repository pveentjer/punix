#include <stdint.h>
#include "screen.h"
#include "sched.h"

struct task_struct task1, task2;

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile ("nop");
    }
}


void task_entry1(void)
{
    uint64_t i = 0;
    for (;;)
    {
        for (int k = 0; k < 5; k++)
        {
            screen_print("Task1 run: ");
            screen_put_uint64(i++);
            screen_put_char('\n');

            delay(200000000);
        }

        yield();
    }
}

void task_entry2(void)
{
    uint64_t i = 0;
    for (;;)
    {
        for (int k = 0; k < 5; k++)
        {
            screen_print("---Task2 run: ");
            screen_put_uint64(i++);
            screen_put_char('\n');

            delay(200000000);
        }

        yield();
    }
}

static void print_bootmsg(void)
{
    screen_println("Munix 0.001");
}



/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    screen_clear();
    print_bootmsg();

    sched_init();

    task1.pid = 0;
    task1.eip = (uint32_t) task_entry1;
    task1.esp = 0x90000;
    task1.ebp = 0x90000;
    task1.next = NULL;

    task2.pid = 1;
    task2.eip = (uint32_t) task_entry2;
    task2.esp = 0x80000;
    task2.ebp = 0x80000;
    task2.next = NULL;

    sched_add_task(&task1);
    sched_add_task(&task2);

    sched_start();
}
