#include "kernel/kutils.h"
#include "kernel/console.h"

void *k_memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = s[i];
    }

    return dest;
}

void *k_memset(void *dest, int value, size_t n)
{
    unsigned char *d = dest;

    for (size_t i = 0; i < n; i++)
    {
        d[i] = (unsigned char) value;
    }

    return dest;
}

int k_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b))
    {
        a++;
        b++;
    }
    return (unsigned char) *a - (unsigned char) *b;
}

int k_strncmp(const char *s1, const char *s2, size_t n)
{
    for (size_t i = 0; i < n; i++)
    {
        unsigned char c1 = (unsigned char)s1[i];
        unsigned char c2 = (unsigned char)s2[i];

        if (c1 != c2 || c1 == '\0' || c2 == '\0')
        {
            return c1 - c2;
        }
    }

    return 0;
}

char *k_strcat(char *dest, const char *src)
{
    char *d = dest;

    // Move d to the end of the existing string
    while (*d)
        d++;

    // Copy characters from src (including '\0')
    while ((*d++ = *src++))
        ;

    return dest;
}

char *k_strcpy(char *dest, const char *src)
{
    char *d = dest;

    while ((*d++ = *src++))
    {}   // copy each character including the terminating '\0'

    return dest;
}

char *k_strncpy(char *dest, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
    {
        dest[i] = src[i];
    }

    // Pad with nulls if src was shorter than n
    for (; i < n; i++)
    {
        dest[i] = '\0';
    }

    return dest;
}

char *k_strrchr(const char *s, int c)
{
    const char *last = NULL;
    char ch = (char)c;

    while (*s)
    {
        if (*s == ch)
        {
            last = s;
        }
        s++;
    }

    return (char *)last;
}


void k_itoa(int value, char *str)
{
    char tmp[16];
    int i = 0, j = 0;

    if (value == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int neg = 0;
    if (value < 0)
    {
        neg = 1;
        value = -value;
    }

    while (value > 0 && i < 15)
    {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    if (neg)
        tmp[i++] = '-';

    // reverse
    while (i > 0)
    {
        str[j++] = tmp[--i];
    }
    str[j] = '\0';
}


size_t k_strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
    {
        len++;
    }
    return len;
}

void panic(char *msg)
{
    kprintf("Kernel Panic!!!\n");
    kprintf(msg);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}

size_t u64_to_str(uint64_t value, char *buf, size_t buf_size)
{
    char tmp[32];
    size_t i = 0;

    if (!buf || buf_size == 0)
    {
        return 0;
    }

    if (value == 0)
    {
        if (buf_size < 2)
        {
            return 0;
        }
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    while (value > 0 && i < sizeof(tmp))
    {
        tmp[i++] = (char)('0' + (value % 10u));
        value /= 10u;
    }

    if (i == 0)
    {
        if (buf_size < 2)
        {
            return 0;
        }
        buf[0] = '0';
        buf[1] = '\0';
        return 1;
    }

    size_t len = (i < buf_size - 1) ? i : (buf_size - 1);
    for (size_t j = 0; j < len; j++)
    {
        buf[j] = tmp[i - 1 - j];
    }
    buf[len] = '\0';
    return len;
}