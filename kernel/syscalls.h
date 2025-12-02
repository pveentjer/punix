#ifndef SYSCALLS_H
#define SYSCALLS_H

#include <stdint.h>
#include <stddef.h>   // for size_t

// Minimal ssize_t for your freestanding kernel
typedef long ssize_t;

// POSIX-style fd numbers
#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

ssize_t write(int fd, const void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count);

void yield(void);

void exit(int status);

size_t strlen(const char *s);

int printf(const char *fmt, ...);

void delay(uint32_t count);

#endif // SYSCALLS_H
