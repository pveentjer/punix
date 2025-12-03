#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "vga.h"
#include "sched.h"
#include "syscalls.h"
#include "interrupt.h"
#include "keyboard.h"

void delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __asm__ volatile ("nop");
    }
}


size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0') {
        len++;
    }
    return len;
}


#define STDOUT 1
#define PRINTF_BUF_SIZE 512

static void buf_putc(char *buf, size_t *len, char c)
{
    if (*len < PRINTF_BUF_SIZE) {
        buf[*len] = c;
        (*len)++;
    }
}

static void buf_puts(char *buf, size_t *len, const char *s)
{
    if (!s) return;

    while (*s && *len < PRINTF_BUF_SIZE) {
        buf[*len] = *s;
        (*len)++;
        s++;
    }
}

static void buf_put_uint(char *buf, size_t *len, unsigned int n, unsigned int base)
{
    char tmp[32];
    const char digits[] = "0123456789abcdef";
    int i = 0;

    if (n == 0) {
        tmp[i++] = '0';
    } else {
        while (n > 0 && i < (int)sizeof(tmp)) {
            tmp[i++] = digits[n % base];
            n /= base;
        }
    }

    // reverse into main buffer
    while (i-- > 0 && *len < PRINTF_BUF_SIZE) {
        buf[*len] = tmp[i];
        (*len)++;
    }
}

int printf(const char *fmt, ...)
{
    char buf[PRINTF_BUF_SIZE];
    size_t len = 0;

    va_list args;
    va_start(args, fmt);

    for (const char *p = fmt; *p && len < PRINTF_BUF_SIZE; p++) {
        if (*p != '%') {
            buf_putc(buf, &len, *p);
            continue;
        }

        p++; // skip '%'
        if (!*p) break; // stray '%' at end

        switch (*p) {
            case 's': {
                const char *s = va_arg(args, const char *);
                buf_puts(buf, &len, s ? s : "(null)");
                break;
            }
            case 'd': {
                int val = va_arg(args, int);
                if (val < 0) {
                    buf_putc(buf, &len, '-');
                    val = -val;
                }
                buf_put_uint(buf, &len, (unsigned int)val, 10);
                break;
            }
            case 'u': {
                unsigned int val = va_arg(args, unsigned int);
                buf_put_uint(buf, &len, val, 10);
                break;
            }
            case 'x': {
                unsigned int val = va_arg(args, unsigned int);
                buf_put_uint(buf, &len, val, 16);
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                buf_putc(buf, &len, c);
                break;
            }
            case '%': {
                buf_putc(buf, &len, '%');
                break;
            }
            default: {
                // Unknown format: print it literally
                buf_putc(buf, &len, '%');
                buf_putc(buf, &len, *p);
                break;
            }
        }
    }

    va_end(args);

    if (len > 0) 
    {
        write(FD_STDOUT, buf, len);
    }

    return (int)len;
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;
    
    // Skip whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n') {
        str++;
    }
    
    // Handle sign
    if (*str == '-') {
        sign = -1;
        str++;
    } else if (*str == '+') {
        str++;
    }
    
    // Convert digits
    while (*str >= '0' && *str <= '9') {
        result = result * 10 + (*str - '0');
        str++;
    }
    
    return sign * result;
}


/* Kernel entry point */
__attribute__((noreturn, section(".start")))
void kmain(void)
{
    screen_clear();
    screen_println("Munix 0.001");
    
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
