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

// open() flags (minimal subset)
#define O_RDONLY   0x0
#define O_WRONLY   0x1
#define O_RDWR     0x2
#define O_CREAT    0x40
#define O_TRUNC    0x200

// mode bits (minimal subset)
#define S_IRUSR    0x100
#define S_IWUSR    0x80

// hack because there is no proc fs
int sched_get_tasks(char *buf, int buf_size);

ssize_t write(int fd, const void *buf, size_t count);
ssize_t read(int fd, void *buf, size_t count);

pid_t getpid(void);

pid_t sched_add_task(const char *filename, int argc, char **argv);
pid_t fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);

void sched_yield(void);
void exit(int status);

int kill(pid_t pid, int sig);
int nice(int inc);

// File syscalls
int open(const char *pathname, int flags, int mode);
int close(int fd);

// libc helpers
int strcmp(const char *s1, const char *s2);
size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
int atoi(const char *str);
int printf(const char *fmt, ...);
void delay(uint32_t count);

#endif // LIBC_H
