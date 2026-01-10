#ifndef KUTILS_H
#define KUTILS_H

#include "sys/types.h"
#include <stdarg.h>
#include <stdint.h>

/* ------------------------------------------------------------
 * Basic memory operations
 * ------------------------------------------------------------ */

void *k_memcpy(void *dest, const void *src, size_t n);

void *k_memset(void *dest, int value, size_t n);

void *k_memmove(void *dest, const void *src, size_t n);

int k_strcmp(const char *a, const char *b);

int k_strncmp(const char *s1, const char *s2, size_t n);

char *k_strchr(const char *s, int c);

char *k_strcat(char *dest, const char *src);

int k_vsnprintf(char *buf, size_t size, const char *fmt, va_list ap);

int k_snprintf(char *buf, size_t size, const char *fmt, ...);

char* k_strcpy(char *dest, const char *src);

char *k_strncpy(char *dest, const char *src, size_t n);

char *k_strrchr(const char *str, int c);

const char *k_strstr(const char *haystack, const char *needle);

void k_itoa(int value, char *str);

void k_itoa_hex(uint32_t value, char *str);

int k_atoi(const char *s);

size_t k_strlen(const char *s);

void panic(char* msg);

static inline uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}


size_t u64_to_str(uint64_t value, char *buf, size_t buf_size);

#endif // KUTILS_H
