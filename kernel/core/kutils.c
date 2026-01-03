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

char *k_strchr(const char *s, int c)
{
    if (!s)
    {
        return NULL;
    }

    while (*s != '\0')
    {
        if (*s == (char)c)
        {
            return (char *)s;
        }
        s++;
    }

    // strchr also matches the null terminator
    if ((char)c == '\0')
    {
        return (char *)s;
    }

    return NULL;
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

void k_itoa_hex(uint32_t value, char *str)
{
    const char hex_digits[] = "0123456789abcdef";
    char temp[9]; // 8 hex digits + null terminator
    int i = 0;

    // Handle 0 specially
    if (value == 0)
    {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    // Build string in reverse
    while (value > 0)
    {
        temp[i++] = hex_digits[value & 0xF];
        value >>= 4;
    }

    // Reverse into output string
    int j = 0;
    while (i > 0)
    {
        str[j++] = temp[--i];
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


const char *k_strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle)
        return NULL;

    // Empty needle matches at the beginning
    if (*needle == '\0')
        return haystack;

    // Search for needle in haystack
    while (*haystack)
    {
        const char *h = haystack;
        const char *n = needle;

        // Try to match needle starting at current position
        while (*h && *n && *h == *n)
        {
            h++;
            n++;
        }

        // If we reached end of needle, we found a match
        if (*n == '\0')
            return haystack;

        haystack++;
    }

    return NULL;
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
int k_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap)
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

int k_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int ret = k_vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return ret;
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
