#include "../../include/kernel/kutils.h"
#include "../../include/kernel/vga.h"

void *k_memcpy(void *dest, const void *src, size_t n)
{
    unsigned char *d = dest;
    const unsigned char *s = src;

    for (size_t i = 0; i < n; i++)
        d[i] = s[i];

    return dest;
}

void *k_memset(void *dest, int value, size_t n)
{
    unsigned char *d = dest;

    for (size_t i = 0; i < n; i++)
        d[i] = (unsigned char)value;

    return dest;
}

int k_strcmp(const char *a, const char *b)
{
    while (*a && (*a == *b)) {
        a++;
        b++;
    }
    return (unsigned char)*a - (unsigned char)*b;
}

void k_itoa(int value, char *str)
{
    char tmp[16];
    int i = 0, j = 0;

    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return;
    }

    int neg = 0;
    if (value < 0) {
        neg = 1;
        value = -value;
    }

    while (value > 0 && i < 15) {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    }

    if (neg)
        tmp[i++] = '-';

    // reverse
    while (i > 0)
        str[j++] = tmp[--i];
    str[j] = '\0';
}


size_t k_strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

void panic(char* msg)
{
    screen_printf("Kernel Panic!!!\n");
    screen_println(msg);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}