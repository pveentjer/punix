#ifndef KUTILS_H
#define KUTILS_H

#include <stddef.h>   // for size_t
#include <stdint.h>

/* ------------------------------------------------------------
 * Basic memory operations
 * ------------------------------------------------------------ */

void *k_memcpy(void *dest, const void *src, size_t n);

void *k_memset(void *dest, int value, size_t n);

void *k_memmove(void *dest, const void *src, size_t n);

int k_strcmp(const char *a, const char *b);

int k_strncmp(const char *s1, const char *s2, size_t n);

char *k_strcat(char *dest, const char *src);

char* k_strcpy(char *dest, const char *src);

char *k_strncpy(char *dest, const char *src, size_t n);

char *k_strrchr(const char *str, int c);

void k_itoa(int value, char *str);

size_t k_strlen(const char *s);

void panic(char* msg);

static inline uint32_t align_up(uint32_t value, uint32_t align)
{
    return (value + align - 1) & ~(align - 1);
}

static inline uint32_t align_down(uint32_t value, uint32_t align)
{
    return value & ~(align - 1);
}

static inline uint32_t read_esp(void)
{
    uint32_t esp;
    __asm__ volatile("movl %%esp, %0" : "=r"(esp));
    return esp;
}

static inline uint16_t read_cs(void)
{
    uint16_t cs;
    __asm__ volatile ("mov %%cs, %0" : "=r"(cs));
    return cs;
}

static inline uint16_t read_ds(void)
{
    uint16_t ds;
    __asm__ volatile ("mov %%ds, %0" : "=r"(ds));
    return ds;
}

static inline uint16_t read_es(void)
{
    uint16_t es;
    __asm__ volatile ("mov %%es, %0" : "=r"(es));
    return es;
}

static inline uint16_t read_fs(void)
{
    uint16_t fs;
    __asm__ volatile ("mov %%fs, %0" : "=r"(fs));
    return fs;
}

static inline uint16_t read_gs(void)
{
    uint16_t gs;
    __asm__ volatile ("mov %%gs, %0" : "=r"(gs));
    return gs;
}

static inline uint16_t read_ss(void)
{
    uint16_t ss;
    __asm__ volatile ("mov %%ss, %0" : "=r"(ss));
    return ss;
}

size_t u64_to_str(uint64_t value, char *buf, size_t buf_size);

#endif // KUTILS_H
