#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "kernel/libc.h"
#include "kernel/kernel_api.h"


void delay(uint32_t count)
{
    for (volatile uint32_t i = 0; i < count; i++)
    {
        __asm__ volatile ("nop");
    }
}

char *strcpy(char *dest, const char *src)
{
    char *d = dest;

    while ((*d++ = *src++))
    {}   // copy each character including the terminating '\0'

    return dest;
}

size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    return len;
}


#define STDOUT 1
#define PRINTF_BUF_SIZE 512

static void buf_putc(char *buf, size_t *len, char c)
{
    if (*len < PRINTF_BUF_SIZE)
    {
        buf[*len] = c;
        (*len)++;
    }
}

static void buf_puts(char *buf, size_t *len, const char *s)
{
    if (!s) return;

    while (*s && *len < PRINTF_BUF_SIZE)
    {
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

    if (n == 0)
    {
        tmp[i++] = '0';
    }
    else
    {
        while (n > 0 && i < (int) sizeof(tmp))
        {
            tmp[i++] = digits[n % base];
            n /= base;
        }
    }

    // reverse into main buffer
    while (i-- > 0 && *len < PRINTF_BUF_SIZE)
    {
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

    for (const char *p = fmt; *p && len < PRINTF_BUF_SIZE; p++)
    {
        if (*p != '%')
        {
            buf_putc(buf, &len, *p);
            continue;
        }

        p++; // skip '%'
        if (!*p) break; // stray '%' at end

        switch (*p)
        {
            case 's':
            {
                const char *s = va_arg(args, const char *);
                buf_puts(buf, &len, s ? s : "(null)");
                break;
            }
            case 'd':
            {
                int val = va_arg(args, int);
                if (val < 0)
                {
                    buf_putc(buf, &len, '-');
                    val = -val;
                }
                buf_put_uint(buf, &len, (unsigned int) val, 10);
                break;
            }
            case 'u':
            {
                unsigned int val = va_arg(args, unsigned int);
                buf_put_uint(buf, &len, val, 10);
                break;
            }
            case 'x':
            {
                unsigned int val = va_arg(args, unsigned int);
                buf_put_uint(buf, &len, val, 16);
                break;
            }
            case 'c':
            {
                char c = (char) va_arg(args, int);
                buf_putc(buf, &len, c);
                break;
            }
            case '%':
            {
                buf_putc(buf, &len, '%');
                break;
            }
            default:
            {
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

    return (int) len;
}

int atoi(const char *str)
{
    int result = 0;
    int sign = 1;

    // Skip whitespace
    while (*str == ' ' || *str == '\t' || *str == '\n')
    {
        str++;
    }

    // Handle sign
    if (*str == '-')
    {
        sign = -1;
        str++;
    }
    else if (*str == '+')
    {
        str++;
    }

    // Convert digits
    while (*str >= '0' && *str <= '9')
    {
        result = result * 10 + (*str - '0');
        str++;
    }

    return sign * result;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    return kapi()->sys_write(fd, buf, count);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return kapi()->sys_read(fd, buf, count);
}

pid_t getpid(void)
{
    return kapi()->sys_getpid();
}

void sched_yield(void)
{
    kapi()->sys_yield();
}

void exit(int status)
{
    kapi()->sys_exit(status);
}

int kill(pid_t pid, int sig)
{
    kapi()->sys_kill(pid, sig);
}

int nice(int inc)
{
    kapi()->sys_nice(inc);
}

void sched_add_task(const char *filename, int argc, char **argv)
{
    kapi()->sys_add_task(filename, argc, argv);
}

pid_t fork(void)
{
    kapi()->sys_fork();
}

int execve(const char *pathname, char *const argv[], char *const envp[])
{
    return kapi()->sys_execve(pathname, argv, envp);
}