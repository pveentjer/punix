#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>   // for size_t
#include "dirent.h"
#include "fcntl.h"

// Minimal ssize_t for your freestanding kernel
typedef long ssize_t;

typedef int pid_t;


// mode bits (minimal subset)
#define S_IRUSR    0x100
#define S_IWUSR    0x80


pid_t getpid(void);

pid_t sched_add_task(const char *filename, int argc, char **argv);
pid_t fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);

void sched_yield(void);
void exit(int status);

int kill(pid_t pid, int sig);
int nice(int inc);
pid_t waitpid(pid_t pid, int *status, int options);

void *memcpy(void *dest, const void *src, size_t n);

// libc helpers
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
int atoi(const char *str);
int printf(const char *fmt, ...);
void delay(uint32_t count);

#endif // LIBC_H
