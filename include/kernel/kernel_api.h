#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stddef.h>
#include "dirent.h"

typedef long ssize_t;
typedef int pid_t;

struct kernel_api
{
    ssize_t (*sys_write)(int fd, const char *buf, size_t count);

    ssize_t (*sys_read)(int fd, void *buf, size_t count);

    pid_t (*sys_getpid)(void);

    void (*sys_yield)(void);

    void (*sys_exit)(int status);

    int (*sys_kill)(pid_t pid, int sig);

    pid_t (*sys_fork)(void);

    int (*sys_nice)(int inc);

    int (*sys_execve)(const char *pathname,char *const argv[],char *const envp[]);

    pid_t (*sys_add_task)(const char *filename, int tty_id, char **argv, char **envp);

    pid_t (*sys_waitpid)(pid_t pid, int *status, int options);

    int (*sys_open)(const char *pathname, int flags, int mode);

    int (*sys_close)(int fd);

    int (*sys_getdents)(int fd, struct dirent *buf, unsigned int count);
};

/* 1 MiB base where the kernel header lives */
#define MB(x)               ((x) * 1024u * 1024u)
#define KERNEL_HEADER_ADDR  MB(1)

/* First word in the header: pointer to kernel_api_instance */
#define KERNEL_API_PTR_ADDR ((struct kernel_api * const *)KERNEL_HEADER_ADDR)

static inline struct kernel_api *kapi(void)
{
    return *KERNEL_API_PTR_ADDR;
}

#endif /* KERNEL_API_H */
