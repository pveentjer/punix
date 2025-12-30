#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>   // for size_t

void *memcpy(void *dest, const void *src, size_t n);

int atoi(const char *str);

#endif // LIBC_H
