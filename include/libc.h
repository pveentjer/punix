#ifndef LIBC_H
#define LIBC_H

#include <stdint.h>
#include <stddef.h>   // for size_t
#include "dirent.h"
#include "kernel/fcntl.h"

#define WNOHANG 0x01

// Minimal ssize_t for your freestanding kernel
typedef long ssize_t;

typedef int pid_t;

extern char **environ;

char *getenv(const char *name);

pid_t getpid(void);

pid_t sched_add_task(const char *filename, int tty_id, char **argv, char **envp);

pid_t fork(void);

int execve(const char *pathname, char *const argv[], char *const envp[]);

void sched_yield(void);

void exit(int status);

int kill(pid_t pid, int sig);

int nice(int inc);

pid_t waitpid(pid_t pid, int *status, int options);

void *sbrk(intptr_t increment);

int brk(void *addr);

int chdir(const char *path);

char *getcwd(char *buf, size_t size);

void *memcpy(void *dest, const void *src, size_t n);

// libc helpers

int strcmp(const char *s1, const char *s2);

size_t strlen(const char *s);

char *strchr(const char *s, int c);

char *strcpy(char *dest, const char *src);

char *strcat(char *dest, const char *src);

char *strncpy(char *dest, const char *src, size_t n);

int strncmp(const char *s1, const char *s2, size_t n);

int atoi(const char *str);

int printf(const char *fmt, ...);

void delay(uint32_t count);


#endif // LIBC_H
