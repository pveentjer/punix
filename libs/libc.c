#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "kernel/libc.h"
#include "kernel/sys_calls.h"
#include "kernel/entry.h"

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


char **environ = NULL;

char *getenv(const char *name)
{
    if (name == NULL || *name == '\0' || strchr(name, '=') != NULL)
    {
        return NULL;
    }

    size_t len = strlen(name);

    for (char **env = environ; *env != NULL; env++)
    {
        if (strncmp(*env, name, len) == 0 && (*env)[len] == '=')
        {
            return *env + len + 1;
        }
    }

    return NULL;
}

char *strchr(const char *s, int c)
{
    while (*s != (char) c)
    {
        if (*s == '\0')
            return NULL;
        s++;
    }
    return (char *) s;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i = 0;

    /* Copy up to n characters or until null terminator */
    for (; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }

    /* Pad with '\0' if src was shorter than n */
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }

    return dest;
}

char *strcat(char *dest, const char *src)
{
    char *d = dest;

    /* Move to the end of dest */
    while (*d)
    {
        d++;
    }

    /* Copy src including the null terminator */
    while ((*d++ = *src++) != '\0')
    {
        /* empty */
    }

    return dest;
}

void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *) dest;
    const unsigned char *s = (const unsigned char *) src;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
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

int strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        unsigned char c1 = (unsigned char) s1[i];
        unsigned char c2 = (unsigned char) s2[i];

        if (c1 != c2)
            return c1 - c2;

        if (c1 == '\0')
            return 0;
    }
    return 0;
}

int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2))
    {
        s1++;
        s2++;
    }

    return (unsigned char) *s1 - (unsigned char) *s2;
}

ssize_t write(int fd, const void *buf, size_t count)
{
    sys_enter_kernel_mode();
    ssize_t res = sys()->write(fd, buf, count);
    sys_leave_kernel_mode();
    return res;
}

ssize_t read(int fd, void *buf, size_t count)
{
    sys_enter_kernel_mode();
    ssize_t res = sys()->read(fd, buf, count);
    sys_leave_kernel_mode();
    return res;
}

pid_t getpid(void)
{
    sys_enter_kernel_mode();
    pid_t res = sys()->getpid();
    sys_leave_kernel_mode();
    return res;
}

void sched_yield(void)
{
    sys_enter_kernel_mode();
    sys()->sched_yield();
    sys_leave_kernel_mode();
}

void exit(int status)
{
    sys_enter_kernel_mode();
    sys()->exit(status);
    sys_leave_kernel_mode();
}

int kill(pid_t pid, int sig)
{
    sys_enter_kernel_mode();
    int res = sys()->kill(pid, sig);
    sys_leave_kernel_mode();
    return res;
}

int nice(int inc)
{
    sys_enter_kernel_mode();
    int res = sys()->nice(inc);
    sys_leave_kernel_mode();
    return res;
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    sys_enter_kernel_mode();
    pid_t res = sys()->waitpid(pid, status, options);
    sys_leave_kernel_mode();
    return res;
}

pid_t sched_add_task(const char *filename, int tty_id, char **argv, char **envp)
{
    sys_enter_kernel_mode();
    pid_t res = sys()->add_task(filename, tty_id, argv, envp);
    sys_leave_kernel_mode();
    return res;
}

pid_t fork(void)
{
    sys_enter_kernel_mode();
    pid_t res = sys()->fork();
    sys_leave_kernel_mode();
    return res;
}

int execve(const char *pathname, char *const argv[], char *const envp[])
{
    sys_enter_kernel_mode();
    int res = sys()->execve(pathname, argv, envp);
    sys_leave_kernel_mode();
    return res;
}

int open(const char *pathname, int flags, int mode)
{
    sys_enter_kernel_mode();
    int res = sys()->open(pathname, flags, mode);
    sys_leave_kernel_mode();
    return res;
}

int close(int fd)
{
    sys_enter_kernel_mode();
    int res = sys()->close(fd);
    sys_leave_kernel_mode();
    return res;
}

int getdents(int fd, struct dirent *buf, unsigned int count)
{
    sys_enter_kernel_mode();
    int res = sys()->getdents(fd, buf, count);
    sys_leave_kernel_mode();
    return res;
}

int chdir(const char *path)
{
    sys_enter_kernel_mode();
    int res = sys()->chdir(path);
    sys_leave_kernel_mode();
    return res;
}

char *getcwd(char *buf, size_t size)
{
    sys_enter_kernel_mode();
    char *res = sys()->getcwd(buf, size);
    sys_leave_kernel_mode();
    return res;
}

// This value is initialized by the kernel
void *__curbrk = 0;

int brk(void *addr)
{
    sys_enter_kernel_mode();
    int res = sys()->brk(addr);
    sys_leave_kernel_mode();
    return res;
}

void *sbrk(intptr_t increment)
{
    void *old_brk = __curbrk;

    if (increment != 0)
    {
        void *new_brk = (char *) old_brk + increment;

        sys_enter_kernel_mode();
        int rc = sys()->brk(new_brk);
        sys_leave_kernel_mode();

        if (rc == 0)
        {
            __curbrk = new_brk;
        }
        else
        {
            return (void *) -1;
        }
    }

    return old_brk;
}
