#ifndef STDIO_H
#define STDIO_H

#include "sys/types.h"

int printf(const char *fmt, ...);

int snprintf(char *str, size_t size, const char *fmt, ...);

#endif /* STDIO_H */
