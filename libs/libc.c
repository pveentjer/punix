#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include "kernel/libc.h"
#include "kernel/syscall.h"

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

/* ------------------------------------------------------------------
 * Syscall helpers (x86-32 Linux-style register ABI, up to 5 args)
 *
 *   EAX = nr
 *   EBX = a1
 *   ECX = a2
 *   EDX = a3
 *   ESI = a4
 *   EDI = a5
 *
 * Return:
 *   EAX = ret
 * ------------------------------------------------------------------ */

static inline uint32_t syscall_0(uint32_t nr)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = 0, ecx = 0, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline uint32_t syscall_1(uint32_t nr, uint32_t a1)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = 0, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline uint32_t syscall_2(uint32_t nr, uint32_t a1, uint32_t a2)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = 0, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline uint32_t syscall_3(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = 0, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline uint32_t syscall_4(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = a4, edi = 0;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}

static inline uint32_t syscall_5(uint32_t nr, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
    sys_enter_fn_t fn = sys_enter_fn();

    uint32_t eax = nr;
    uint32_t ebx = a1, ecx = a2, edx = a3, esi = a4, edi = a5;

    __asm__ volatile(
            "call *%[fn]"
            : "+a"(eax), "+b"(ebx), "+c"(ecx), "+d"(edx), "+S"(esi), "+D"(edi)
            : [fn] "r"(fn)
    : "memory", "cc"
    );

    return eax;
}


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

/* brk / sbrk */

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
