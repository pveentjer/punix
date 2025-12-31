#include <stdint.h>
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

void *k_memmove(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    if (d < s)
    {
        /* Copy forward */
        while (n--)
        {
            *d++ = *s++;
        }
    }
    else
    {
        /* Copy backward to handle overlap */
        d += n;
        s += n;
        while (n--)
        {
            *--d = *--s;
        }
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
        unsigned char c1 = (unsigned char) s1[i];
        unsigned char c2 = (unsigned char) s2[i];

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
    {
        d++;
    }

    // Copy characters from src (including '\0')
    while ((*d++ = *src++))
    {}

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
    char ch = (char) c;

    while (*s)
    {
        if (*s == ch)
        {
            last = s;
        }
        s++;
    }

    return (char *) last;
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
    {
        tmp[i++] = '-';
    }

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
        tmp[i++] = (char) ('0' + (value % 10u));
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

/* unsigned 64-bit division */
uint64_t __udivdi3(uint64_t n, uint64_t d)
{
    if (d == 0)
    {
        return 0;
    }

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
    if (d == 0)
    {
        return 0;
    }

    uint64_t r = 0;

    for (int i = 63; i >= 0; i--)
    {
        r <<= 1;
        r |= (n >> i) & 1;
        if (r >= d)
        {
            r -= d;
        }
    }

    return r;
}

/* signed 64-bit division */
int64_t __divdi3(int64_t a, int64_t b)
{
    int neg = 0;
    if (a < 0)
    {
        a = -a;
        neg ^= 1;
    }
    if (b < 0)
    {
        b = -b;
        neg ^= 1;
    }

    uint64_t q = __udivdi3((uint64_t) a, (uint64_t) b);
    return neg ? -(int64_t) q : (int64_t) q;
}

/* signed 64-bit modulo */
int64_t __moddi3(int64_t a, int64_t b)
{
    int neg = (a < 0);
    if (a < 0)
    {
        a = -a;
    }
    if (b < 0)
    {
        b = -b;
    }

    uint64_t r = __umoddi3((uint64_t) a, (uint64_t) b);
    return neg ? -(int64_t) r : (int64_t) r;
}

/* combined div+mod helper (some GCC paths use this) */
int64_t __divmoddi4(int64_t a, int64_t b, int64_t *rem)
{
    int64_t q = __divdi3(a, b);
    if (rem)
    {
        *rem = a - q * b;
    }
    return q;
}


/*
 * unsigned 64-bit div+mod helper
 * GCC may emit this instead of separate calls
 */
uint64_t __udivmoddi4(uint64_t n, uint64_t d, uint64_t *rem)
{
    if (d == 0)
    {
        if (rem) *rem = 0;
        return 0;
    }

    uint64_t q = 0;
    uint64_t r = 0;

    for (int i = 63; i >= 0; i--)
    {
        r <<= 1;
        r |= (n >> i) & 1ULL;

        if (r >= d)
        {
            r -= d;
            q |= (1ULL << i);
        }
    }

    if (rem)
        *rem = r;

    return q;
}
