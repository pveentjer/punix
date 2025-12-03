#ifndef KUTILS_H
#define KUTILS_H

#include <stddef.h>   // for size_t

/* ------------------------------------------------------------
 * Basic memory operations
 * ------------------------------------------------------------ */

void *k_memcpy(void *dest, const void *src, size_t n);
void *k_memset(void *dest, int value, size_t n);

#endif // KUTILS_H
