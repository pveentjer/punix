#ifndef SYS_CALLS_H
#define SYS_CALLS_H

#include <stddef.h>
#include "dirent.h"

typedef long ssize_t;
typedef int pid_t;

struct sys_calls
{
    ssize_t (*write)(int fd, const char *buf, size_t count);

    ssize_t (*read)(int fd, void *buf, size_t count);

    pid_t (*getpid)(void);

    void (*sched_yield)(void);

    void (*exit)(int status);

    int (*kill)(pid_t pid, int sig);

    pid_t (*fork)(void);

    int (*nice)(int inc);

    int (*execve)(const char *pathname, char *const argv[], char *const envp[]);

    pid_t (*add_task)(const char *filename, int tty_id, char **argv, char **envp);

    pid_t (*waitpid)(pid_t pid, int *status, int options);

    int (*open)(const char *pathname, int flags, int mode);

    int (*close)(int fd);

    int (*getdents)(int fd, struct dirent *buf, unsigned int count);

    int (*brk)(void *addr);

    int (*chdir)(const char *path);

    char* (*getcwd)(char *buf, size_t size);
};

/* 1 MiB base where the kernel header lives */
#define MB(x)               ((x) * 1024u * 1024u)
#define SYS_CALLS_HDR_ADDR  MB(1)

/* First word in the header: pointer to sys_call_instance */
#define SYS_CALLS_PTR_ADDR ((struct sys_calls * const *)SYS_CALLS_HDR_ADDR)

/* Placeholder: will later do real segment switching */
static inline void sys_enter_kernel_mode(void)
{
    /* TODO: implement segmentation switch to kernel view */
}

static inline void sys_leave_kernel_mode(void)
{
    /* TODO: implement segmentation switch back to app view */
}

static inline struct sys_calls *sys(void)
{
    return *SYS_CALLS_PTR_ADDR;
}

#endif /* SYS_CALLS_H */
