#ifndef UNISTD_H
#define UNISTD_H

#include <stdint.h>
#include <stddef.h>

#define WNOHANG 0x01

typedef long ssize_t;
typedef int  pid_t;

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

pid_t wait(int *status);

void *sbrk(intptr_t increment);

int brk(void *addr);

int chdir(const char *path);

char *getcwd(char *buf, size_t size);

void delay(uint32_t count);

int setctty(int tty_id);


#endif /* UNISTD_H */
