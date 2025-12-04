#ifndef KUTILS_H
#define KUTILS_H

#include <stddef.h>   // for size_t
#include <stdint.h>

/* ------------------------------------------------------------
 * Basic memory operations
 * ------------------------------------------------------------ */

void *k_memcpy(void *dest, const void *src, size_t n);

void *k_memset(void *dest, int value, size_t n);

int k_strcmp(const char *a, const char *b);

void panic(char* msg);

static inline uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}

#endif // KUTILS_H
