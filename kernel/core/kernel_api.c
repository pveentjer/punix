#include "../../include/kernel/kernel_api.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include <stdint.h>

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

static void screen_put_hex32(uint32_t v)
{
    static const char HEX[] = "0123456789ABCDEF";
    char buf[10]; // "0x" + 8 digits
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++) {
        int shift = (7 - i) * 4;
        buf[2 + i] = HEX[(v >> shift) & 0xF];
    }
    for (int i = 0; i < 10; i++) {
        screen_put_char(buf[i]);
    }
}

/* ------------------------------------------------------------
 * k_write / k_read / etc
 * ------------------------------------------------------------ */

static ssize_t k_write(int fd, const char *buf, size_t count)
{
//    screen_println("DBG: inside k_write");

    if (buf == 0 || count == 0)
        return 0;

    switch (fd)
    {
        case FD_STDOUT:
        case FD_STDERR:
            for (size_t i = 0; i < count; i++)
                screen_put_char(buf[i]);
            return (ssize_t) count;

        case FD_STDIN:
            return -1;

        default:
            return -1;
    }
}

static ssize_t k_read(int fd, void *buf, size_t count)
{
    if (fd != FD_STDIN)
        return -1;

    if (!buf || count == 0)
        return 0;

    char *cbuf = (char *) buf;
    size_t read_cnt = 0;

    while (read_cnt < count && keyboard_has_char())
        cbuf[read_cnt++] = keyboard_get_char();

    return (ssize_t) read_cnt;
}

static pid_t k_getpid(void)      { return getpid(); }
static void  k_yield(void)       { yield(); }
static void  k_exit(int status)  { exit(status); }
static void  k_sched_add_task(const char *filename, int argc, char **argv)
{
    sched_add_task(filename, argc, argv);
}

/* ------------------------------------------------------------
 * Exported API instance in its own section
 * ------------------------------------------------------------ */

__attribute__((section(".kernel_api"), used))
const struct kernel_api kernel_api_instance = {
        .write          = k_write,
        .read           = k_read,
        .getpid         = k_getpid,
        .yield          = k_yield,
        .exit           = k_exit,
        .sched_add_task = k_sched_add_task,
};

/* ------------------------------------------------------------
 * Debug: print kernel-side function addresses (hex)
 * ------------------------------------------------------------ */

void kernel_api_debug_dump_words(void)
{
    uint32_t *p = (uint32_t *)&kernel_api_instance;

    screen_println("DBG: kernel_api_instance raw words:");
    for (int i = 0; i < 6; i++) {
        screen_put_hex32(p[i]);   // same helper you already use
        screen_put_char(' ');
    }
    screen_put_char('\n');
}
//
//void kernel_api_debug_print(void)
//{
//    screen_println("DBG: kernel_api_instance / function pointers");
//
//    screen_print("  &kernel_api_instance = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&kernel_api_instance);
//    screen_put_char('\n');
//
//    screen_print("  k_write             = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_write);
//    screen_put_char('\n');
//
//    screen_print("  k_read              = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_read);
//    screen_put_char('\n');
//
//    screen_print("  k_getpid            = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_getpid);
//    screen_put_char('\n');
//
//    screen_print("  k_yield             = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_yield);
//    screen_put_char('\n');
//
//    screen_print("  k_exit              = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_exit);
//    screen_put_char('\n');
//
//    screen_print("  k_sched_add_task    = ");
//    screen_put_hex32((uint32_t)(uintptr_t)&k_sched_add_task);
//    screen_put_char('\n');
//
//    kernel_api_debug_dump_words();
//}
