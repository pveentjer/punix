#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "string.h"
#include "kernel/syscall.h"
#include "fcntl.h"
#include "time.h"
#include "unistd.h"
#include "syscall_arch.h"

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

/* Helper for 64-bit division by 10 (avoids __udivdi3) */
static uint64_t udiv64_10(uint64_t n, unsigned int *rem)
{
    uint64_t q = (n >> 1) + (n >> 2);
    q = q + (q >> 4);
    q = q + (q >> 8);
    q = q + (q >> 16);
    q = q + (q >> 32);
    q = q >> 3;

    unsigned int r = (unsigned int)(n - q * 10);
    if (r >= 10)
    {
        q++;
        r -= 10;
    }

    *rem = r;
    return q;
}

/* Core formatting function - writes to buffer */
int vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
{
    size_t pos = 0;

#define PUT_CHAR(c) do { \
        if (pos < size - 1) buf[pos] = (c); \
        pos++; \
    } while(0)

    while (*fmt && pos < size - 1)
    {
        if (*fmt != '%')
        {
            PUT_CHAR(*fmt++);
            continue;
        }

        fmt++; /* skip '%' */
        if (!*fmt) break;

        /* Parse flags and width */
        int zero_pad = 0;
        int width = 0;

        if (*fmt == '0')
        {
            zero_pad = 1;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9')
        {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* Parse length modifier */
        int is_long = 0;
        int is_long_long = 0;

        if (*fmt == 'l')
        {
            is_long = 1;
            fmt++;
            if (*fmt == 'l')
            {
                is_long_long = 1;
                fmt++;
            }
        }

        if (*fmt == 'z')
        {
            /* size_t modifier - treat as unsigned long */
            is_long = 1;
            fmt++;
        }

        switch (*fmt)
        {
            case '%':
                PUT_CHAR('%');
                break;

            case 'c':
            {
                int c = va_arg(ap, int);
                PUT_CHAR((char)c);
                break;
            }

            case 's':
            {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                while (*s)
                {
                    PUT_CHAR(*s++);
                }
                break;
            }

            case 'd':
            case 'i':
            {
                int v = va_arg(ap, int);
                unsigned int uv;
                int negative = 0;

                if (v < 0)
                {
                    negative = 1;
                    uv = (unsigned int)(-v);
                }
                else
                {
                    uv = (unsigned int)v;
                }

                char tmp[16];
                int tmp_pos = 0;
                if (uv == 0)
                {
                    tmp[tmp_pos++] = '0';
                }
                else
                {
                    while (uv && tmp_pos < 16)
                    {
                        tmp[tmp_pos++] = '0' + (uv % 10);
                        uv /= 10;
                    }
                }

                int num_len = tmp_pos + (negative ? 1 : 0);
                int pad_len = (width > num_len) ? (width - num_len) : 0;

                if (negative)
                {
                    PUT_CHAR('-');
                }

                while (pad_len > 0)
                {
                    PUT_CHAR(zero_pad ? '0' : ' ');
                    pad_len--;
                }

                while (tmp_pos > 0)
                {
                    PUT_CHAR(tmp[--tmp_pos]);
                }
                break;
            }

            case 'u':
            {
                char tmp[24];
                int tmp_pos = 0;

                if (is_long_long)
                {
                    uint64_t uv = va_arg(ap, uint64_t);

                    if (uv == 0)
                    {
                        tmp[tmp_pos++] = '0';
                    }
                    else
                    {
                        while (uv && tmp_pos < 24)
                        {
                            unsigned int rem;
                            uv = udiv64_10(uv, &rem);
                            tmp[tmp_pos++] = '0' + rem;
                        }
                    }
                }
                else
                {
                    unsigned int uv = va_arg(ap, unsigned int);

                    if (uv == 0)
                    {
                        tmp[tmp_pos++] = '0';
                    }
                    else
                    {
                        while (uv && tmp_pos < 24)
                        {
                            tmp[tmp_pos++] = '0' + (uv % 10);
                            uv /= 10;
                        }
                    }
                }

                int pad_len = (width > tmp_pos) ? (width - tmp_pos) : 0;
                while (pad_len > 0)
                {
                    PUT_CHAR(zero_pad ? '0' : ' ');
                    pad_len--;
                }

                while (tmp_pos > 0)
                {
                    PUT_CHAR(tmp[--tmp_pos]);
                }
                break;
            }

            case 'x':
            case 'X':
            {
                char tmp[16];
                int tmp_pos = 0;
                static const char HEX[] = "0123456789ABCDEF";

                if (is_long_long)
                {
                    uint64_t v = va_arg(ap, uint64_t);

                    if (v == 0)
                    {
                        tmp[tmp_pos++] = '0';
                    }
                    else
                    {
                        while (v && tmp_pos < 16)
                        {
                            tmp[tmp_pos++] = HEX[v & 0xF];
                            v >>= 4;
                        }
                    }
                }
                else
                {
                    unsigned int v = va_arg(ap, unsigned int);

                    if (v == 0)
                    {
                        tmp[tmp_pos++] = '0';
                    }
                    else
                    {
                        while (v && tmp_pos < 16)
                        {
                            tmp[tmp_pos++] = HEX[v & 0xF];
                            v >>= 4;
                        }
                    }
                }

                int pad_len = (width > tmp_pos) ? (width - tmp_pos) : 0;
                while (pad_len > 0)
                {
                    PUT_CHAR(zero_pad ? '0' : ' ');
                    pad_len--;
                }

                while (tmp_pos > 0)
                {
                    PUT_CHAR(tmp[--tmp_pos]);
                }
                break;
            }

            case 'p':
            {
                void *ptr = va_arg(ap, void *);
                unsigned int v = (unsigned int)ptr;
                static const char HEX[] = "0123456789abcdef";

                PUT_CHAR('0');
                PUT_CHAR('x');

                for (int i = 7; i >= 0; i--)
                {
                    PUT_CHAR(HEX[(v >> (i * 4)) & 0xF]);
                }
                break;
            }

            default:
                PUT_CHAR('%');
                PUT_CHAR(*fmt);
                break;
        }
        fmt++;
    }

    if (pos < size)
        buf[pos] = '\0';
    else if (size > 0)
        buf[size - 1] = '\0';

#undef PUT_CHAR
    return (int)pos;
}

int snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
}

int printf(const char *fmt, ...)
{
    char buf[512];
    va_list ap;

    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len > 0)
    {
        int to_write = (len < (int)sizeof(buf)) ? len : (int)sizeof(buf) - 1;
        write(FD_STDOUT, buf, to_write);
    }

    return len;
}

extern long __syscall0(long nr);

extern long __syscall1(long nr, long a1);

extern long __syscall2(long nr, long a1, long a2);

extern long __syscall3(long nr, long a1, long a2, long a3);

extern long __syscall4(long nr, long a1, long a2, long a3, long a4);

extern long __syscall_5(long nr, long a1, long a2, long a3, long a4, long a5);

extern long __syscall_6(long nr, long a1, long a2, long a3, long a4, long a5);

ssize_t write(int fd, const void *buf, size_t count)
{
    return (ssize_t)__syscall3(SYS_write,
                              (uint32_t)fd,
                              (uint32_t)buf,
                              (uint32_t)count);
}

ssize_t read(int fd, void *buf, size_t count)
{
    return (ssize_t)__syscall3(SYS_read,
                              (uint32_t)fd,
                              (uint32_t)buf,
                              (uint32_t)count);
}

pid_t getpid(void)
{
    return (pid_t)__syscall0(SYS_getpid);
}

void sched_yield(void)
{
    (void)__syscall0(SYS_sched_yield);
}

void exit(int status)
{
    (void)__syscall1(SYS_exit, (uint32_t)status);
}

int kill(pid_t pid, int sig)
{
    return (int)__syscall2(SYS_kill,
                          (uint32_t)pid,
                          (uint32_t)sig);
}

int nice(int inc)
{
    return (int)__syscall1(SYS_nice, (uint32_t)inc);
}

int setctty(int tty_id)
{
    return (int)__syscall1(SYS_setctty, (uint32_t)tty_id);
}


pid_t waitpid(pid_t pid, int *status, int options)
{
    return (pid_t)__syscall3(SYS_waitpid,
                            (uint32_t)pid,
                            (uint32_t)status,
                            (uint32_t)options);
}

pid_t wait(int *status)
{
    return waitpid(-1, status, 0);
}

pid_t fork(void)
{
    return (pid_t)__syscall0(SYS_fork);
}

int execve(const char *pathname, char *const argv[], char *const envp[])
{
    return (int)__syscall3(SYS_execve,
                          (uint32_t)pathname,
                          (uint32_t)argv,
                          (uint32_t)envp);
}

int open(const char *pathname, int flags, int mode)
{
    return (int)__syscall3(SYS_open,
                          (uint32_t)pathname,
                          (uint32_t)flags,
                          (uint32_t)mode);
}

int close(int fd)
{
    return (int)__syscall1(SYS_close, (uint32_t)fd);
}

int getdents(int fd, struct dirent *buf, unsigned int count)
{
    return (int)__syscall3(SYS_getdents,
                          (uint32_t)fd,
                          (uint32_t)buf,
                          (uint32_t)count);
}

int chdir(const char *path)
{
    return (int)__syscall1(SYS_chdir, (uint32_t)path);
}

char *getcwd(char *buf, size_t size)
{
    return (char *)__syscall2(SYS_getcwd,
                             (uint32_t)buf,
                             (uint32_t)size);
}

void *__curbrk = 0;

int brk(void *addr)
{
    return (int)__syscall1(SYS_brk, (uint32_t)addr);
}

void *sbrk(intptr_t increment)
{
    void *old_brk = __curbrk;

    if (increment != 0)
    {
        void *new_brk = (char *)old_brk + increment;
        int rc = (int)__syscall1(SYS_brk, (uint32_t)new_brk);

        if (rc == 0)
            __curbrk = new_brk;
        else
            return (void *)-1;
    }

    return old_brk;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
    return (int)__syscall2(SYS_clock_gettime,
                          (uint32_t)clk_id,
                          (uint32_t)tp);
}

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