#include <stdint.h>
#include "screen.h"
#include "sched.h"

struct run_queue run_queue;
struct task_struct task1, task2;
struct task_struct *current;

static void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile ("nop");
    }
}

static void yield(void)
{
    screen_println("Context switch");

    if (run_queue.len == 0)
    {
        return;
    }

    task_context_store(current);
    run_queue_push(&run_queue, current);
    current = run_queue_poll(&run_queue);
    task_context_load(current);
    task_start(current);
}

void task_entry1(void)
{
    uint64_t i = 0;
    for (;;)
    {
        for (int k = 0; k < 10; k++)
        {
            screen_print("Task1 run: ");
            screen_put_uint64(i++);
            screen_put_char('\n');

            delay(100000000);
        }

        yield();
    }
}

void task_entry2(void)
{
    uint64_t i = 0;
    for (;;)
    {
        for (int k = 0; k < 10; k++)
        {
            screen_print("Task2 run: ");
            screen_put_uint64(i++);
            screen_put_char('\n');

            delay(100000000);
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

    run_queue_init(&run_queue);

    task1.pid = 0;
    task1.eip = (uint32_t) task_entry1;
    task1.esp = 0x90000;
    task1.ebp = 0x90000;
    task1.eflags = 0x202;
    task1.next = NULL;

    task2.pid = 1;
    task2.eip = (uint32_t) task_entry2;
    task2.esp = 0x80000;
    task2.ebp = 0x80000;
    task2.eflags = 0x202;
    task2.next = NULL;

    run_queue_push(&run_queue, &task1);
    run_queue_push(&run_queue, &task2);

    screen_println("run queue length");
    screen_put_uint64(run_queue.len);

    current = run_queue_poll(&run_queue);
    task_start(&task1);
}
