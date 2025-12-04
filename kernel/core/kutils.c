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


void panic(char* msg)
{
    screen_printf("Kernel Panic!!!\n");
    screen_println(msg);

    for (;;)
    {
        __asm__ volatile("hlt");
    }
}