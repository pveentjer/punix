#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>   // for size_t

// Minimal ssize_t for your freestanding kernel
typedef long ssize_t;

typedef int pid_t;

// POSIX-style fd numbers
#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

ssize_t write(int fd, const void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count);

pid_t getpid(void);

void sched_add_task(const char *filename, int argc, char **argv);

pid_t fork(void);

int execve(const char *pathname, char *const argv[], char *const envp[]);

void sched_yield(void);

void exit(int status);

int kill(pid_t pid, int sig);

int nice(int inc);

size_t strlen(const char *s);

char *strcpy(char *dest, const char *src);

int atoi(const char *str);

int printf(const char *fmt, ...);

void delay(uint32_t count);

#endif // LIBC_H
