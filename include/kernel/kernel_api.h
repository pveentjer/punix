#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef int pid_t;

struct kernel_api
{
    ssize_t (*write)(int fd, const char *buf, size_t count);

    ssize_t (*read)(int fd, void *buf, size_t count);

    pid_t (*getpid)(void);

    void (*yield)(void);

    void (*exit)(int status);

    pid_t (*fork)(void);

    int (*execve)(const char *pathname, char *const argv[], char *const envp[]);

    void (*sched_add_task)(const char *filename, int argc, char **argv);
};

/* Fixed address of pointer slot written by the linker header */
#define KERNEL_API_PTR_ADDR  ((struct kernel_api * const *)0x00100000)

/* --- VGA Debug Helpers ---------------------------------------------------- */

static inline volatile uint16_t *VGA = (volatile uint16_t *) 0xB8000;

static inline void vga_put_at(int row, int col, char c, uint8_t attr)
{
    if (row < 0 || row >= 25 || col < 0 || col >= 80)
        return;
    VGA[row * 80 + col] = ((uint16_t) attr << 8) | (uint8_t) c;
}

static inline void vga_puts_at_col(int row, int col, const char *s, uint8_t attr)
{
    int c = col;
    for (int i = 0; s[i] && c < 80; i++, c++)
    {
        vga_put_at(row, c, s[i], attr);
    }
}

static inline int str_len(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* print string right-aligned on given row */
static inline void vga_puts_right(int row, const char *s, uint8_t attr)
{
    int len = str_len(s);
    int col = 80 - len;
    if (col < 0) col = 0;
    vga_puts_at_col(row, col, s, attr);
}

/* build "label: 0xXXXXXXXX" in a small buffer and print right-aligned */
static inline void vga_put_label_hex_right(int row, const char *label, uint32_t value, uint8_t attr)
{
    static const char HEX[16] = "0123456789ABCDEF";
    char buf[32];
    int idx = 0;

    /* copy label */
    while (label[idx] && idx < (int) (sizeof(buf) - 1))
    {
        buf[idx] = label[idx];
        idx++;
    }

    if (idx < (int) (sizeof(buf) - 1)) buf[idx++] = ' ';
    if (idx < (int) (sizeof(buf) - 1)) buf[idx++] = ':';
    if (idx < (int) (sizeof(buf) - 1)) buf[idx++] = ' ';

    if (idx < (int) (sizeof(buf) - 1)) buf[idx++] = '0';
    if (idx < (int) (sizeof(buf) - 1)) buf[idx++] = 'x';

    for (int i = 0; i < 8 && idx < (int) (sizeof(buf) - 1); i++)
    {
        int shift = (7 - i) * 4;
        buf[idx++] = HEX[(value >> shift) & 0xF];
    }

    buf[idx] = '\0';
    vga_puts_right(row, buf, attr);
}

/* --- kapi() Accessor with bottom-right debug dump ------------------------ */


static inline struct kernel_api *kapi(void)
{
    struct kernel_api *api = *KERNEL_API_PTR_ADDR;

//    if (api) {
//        /* existing per-field prints */
//        vga_put_label_hex_right(18, "kapi",            (uint32_t)(uintptr_t)api,                   0x07);
//        vga_put_label_hex_right(19, "write",           (uint32_t)(uintptr_t)api->write,            0x07);
//        vga_put_label_hex_right(20, "read",            (uint32_t)(uintptr_t)api->read,             0x07);
//        vga_put_label_hex_right(21, "getpid",          (uint32_t)(uintptr_t)api->getpid,           0x07);
//        vga_put_label_hex_right(22, "yield",           (uint32_t)(uintptr_t)api->sched_yield,            0x07);
//        vga_put_label_hex_right(23, "exit",            (uint32_t)(uintptr_t)api->exit,             0x07);
//        vga_put_label_hex_right(24, "sched_add_task",  (uint32_t)(uintptr_t)api->sched_add_task,   0x07);
//
//        /* raw words dump (use different rows to avoid overwrite) */
//        uint32_t *p = (uint32_t *)api;
//        for (int i = 0; i < 6; i++) {
//            vga_put_label_hex_right(10 + i, "w[i]", p[i], 0x07);
//        }
//    }

    return api;
}


#endif /* KERNEL_API_H */
