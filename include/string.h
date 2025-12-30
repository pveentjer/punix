#ifndef STRING_H
#define STRING_H

#include <stddef.h>

int strcmp(const char *s1, const char *s2);

size_t strlen(const char *s);

char *strchr(const char *s, int c);

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

int strncmp(const char *s1, const char *s2, size_t n);

#endif //STRING_H
