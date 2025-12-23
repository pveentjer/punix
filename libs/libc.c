#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "kernel/libc.h"
#include "kernel/syscall.h"
#include "kernel/time.h"

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

static void buf_put_ull(char *buf, size_t *len, unsigned long long n, unsigned int base)
{
    char tmp[64];
    const char digits[] = "0123456789abcdef";
    int i = 0;

    if (n == 0ULL) tmp[i++] = '0';
    else
    {
        while (n && i < (int)sizeof(tmp))
        {
            tmp[i++] = digits[n % base];
            n /= base;
        }
    }

    while (i-- > 0 && *len < PRINTF_BUF_SIZE)
        buf[(*len)++] = tmp[i];
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

        p++;
        if (!*p) break;

        int zero_pad = 0;
        int width = 0;

        if (*p == '0')
        {
            zero_pad = 1;
            p++;
        }

        if (*p >= '1' && *p <= '9')
        {
            width = *p - '0';
            p++;
        }

        int is_ll = 0;
        if (*p == 'l' && *(p + 1) == 'l')
        {
            is_ll = 1;
            p += 2;
        }

        switch (*p)
        {
            case 's':
                buf_puts(buf, &len, va_arg(args, const char *));
                break;

            case 'd':
                if (is_ll)
                {
                    long long v = va_arg(args, long long);
                    unsigned long long u = (v < 0) ? (buf_putc(buf,&len,'-'), -v) : v;
                    buf_put_ull(buf, &len, u, 10);
                }
                else
                {
                    int v = va_arg(args, int);
                    unsigned int u = (v < 0) ? (buf_putc(buf,&len,'-'), -v) : v;
                    buf_put_uint(buf, &len, u, 10);
                }
                break;

            case 'u':
                if (is_ll)
                    buf_put_ull(buf, &len, va_arg(args, unsigned long long), 10);
                else
                    buf_put_uint(buf, &len, va_arg(args, unsigned int), 10);
                break;

            case 'x':
                if (is_ll)
                    buf_put_ull(buf, &len, va_arg(args, unsigned long long), 16);
                else
                    buf_put_uint(buf, &len, va_arg(args, unsigned int), 16);
                break;

            case 'c':
                buf_putc(buf, &len, (char)va_arg(args, int));
                break;

            case '%':
                buf_putc(buf, &len, '%');
                break;

            default:
                buf_putc(buf, &len, '%');
                buf_putc(buf, &len, *p);
                break;
        }
    }

    va_end(args);

    if (len)
        write(FD_STDOUT, buf, len);

    return (int)len;
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



extern uint32_t syscall_0(uint32_t nr);

extern uint32_t syscall_1(uint32_t nr, uint32_t a1);

extern uint32_t syscall_2(uint32_t nr, uint32_t a1, uint32_t a2);

extern uint32_t syscall_3(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3);

extern uint32_t syscall_4(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);

extern uint32_t syscall_5(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5);

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)syscall_3(SYS_write,
                              (uint32_t)fd,
                              (uint32_t)buf,
                              (uint32_t)count);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)syscall_3(SYS_read,
                              (uint32_t)fd,
                              (uint32_t)buf,
                              (uint32_t)count);
}

pid_t getpid(void)
{
    return (pid_t)syscall_0(SYS_getpid);
}

void sched_yield(void)
{
    (void)syscall_0(SYS_sched_yield);
}

void exit(int status)
{
    (void)syscall_1(SYS_exit, (uint32_t)status);
}

int kill(pid_t pid, int sig)
{
    return (int)syscall_2(SYS_kill,
                          (uint32_t)pid,
                          (uint32_t)sig);
}

int nice(int inc)
{
    return (int)syscall_1(SYS_nice, (uint32_t)inc);
}

pid_t waitpid(pid_t pid, int *status, int options)
{
    return (pid_t)syscall_3(SYS_waitpid,
                            (uint32_t)pid,
                            (uint32_t)status,
                            (uint32_t)options);
}

pid_t sched_add_task(const char *filename, int tty_id, char **argv, char **envp)
{
    return (pid_t)syscall_4(SYS_add_task,
                            (uint32_t)filename,
                            (uint32_t)tty_id,
                            (uint32_t)argv,
                            (uint32_t)envp);
}

pid_t fork(void)
{
    return (pid_t)syscall_0(SYS_fork);
}

int execve(const char *pathname, char *const argv[], char *const envp[])
{
    return (int)syscall_3(SYS_execve,
                          (uint32_t)pathname,
                          (uint32_t)argv,
                          (uint32_t)envp);
}

int open(const char *pathname, int flags, int mode)
{
    return (int)syscall_3(SYS_open,
                          (uint32_t)pathname,
                          (uint32_t)flags,
                          (uint32_t)mode);
}

int close(int fd)
{
    return (int)syscall_1(SYS_close, (uint32_t)fd);
}

int getdents(int fd, struct dirent *buf, unsigned int count)
{
    return (int)syscall_3(SYS_getdents,
                          (uint32_t)fd,
                          (uint32_t)buf,
                          (uint32_t)count);
}

int chdir(const char *path)
{
    return (int)syscall_1(SYS_chdir, (uint32_t)path);
}

char *getcwd(char *buf, size_t size)
{
    return (char *)syscall_2(SYS_getcwd,
                             (uint32_t)buf,
                             (uint32_t)size);
}

void *__curbrk = 0;

int brk(void *addr)
{
    return (int)syscall_1(SYS_brk, (uint32_t)addr);
}

void *sbrk(intptr_t increment)
{
    void *old_brk = __curbrk;

    if (increment != 0)
    {
        void *new_brk = (char *)old_brk + increment;
        int rc = (int)syscall_1(SYS_brk, (uint32_t)new_brk);

        if (rc == 0)
            __curbrk = new_brk;
        else
            return (void *)-1;
    }

    return old_brk;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    return (int)syscall_2(SYS_clock_gettime,
                          (uint32_t)clk_id,
                          (uint32_t)tp);
}

#include <stdint.h>

/* unsigned 64-bit division */
uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    if (d == 0) return 0;

    uint64_t q = 0;
    uint64_t r = 0;

    for (int i = 63; i >= 0; i--)
    {
        r <<= 1;
        r |= (n >> i) & 1;
        if (r >= d)
        {
            r -= d;
            q |= (1ULL << i);
        }
    }

    return q;
}

/* unsigned 64-bit modulo */
uint64_t __umoddi3(uint64_t n, uint64_t d)
{
    if (d == 0) return 0;

    uint64_t r = 0;

    for (int i = 63; i >= 0; i--)
    {
        r <<= 1;
        r |= (n >> i) & 1;
        if (r >= d)
            r -= d;
    }

    return r;
}

/* signed 64-bit division */
int64_t __divdi3(int64_t a, int64_t b)
{
    int neg = 0;
    if (a < 0) { a = -a; neg ^= 1; }
    if (b < 0) { b = -b; neg ^= 1; }

    uint64_t q = __udivdi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)q : (int64_t)q;
}

/* signed 64-bit modulo */
int64_t __moddi3(int64_t a, int64_t b)
{
    int neg = (a < 0);
    if (a < 0) a = -a;
    if (b < 0) b = -b;

    uint64_t r = __umoddi3((uint64_t)a, (uint64_t)b);
    return neg ? -(int64_t)r : (int64_t)r;
}

uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem)
{
    if (d == 0)
    {
        if (rem) *rem = 0;
        return 0;
    }

    uint64_t q = 0;
    uint64_t r = 0;

    int i = 63;
    do
    {
        r = (r << 1) | ((n >> i) & 1ULL);
        if (r >= d)
        {
            r -= d;
            q |= (1ULL << i);
        }
    }
    while (i--);

    if (rem)
        *rem = r;

    return q;
}

