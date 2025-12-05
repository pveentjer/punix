#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stddef.h>
#include <stdint.h>

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

    int (*sys_execve)(const char *pathname, char *const argv[], char *const envp[]);

    void (*sys_add_task)(const char *filename, int argc, char **argv);
};

/* Fixed address of pointer slot written by the linker header */
#define KERNEL_API_PTR_ADDR  ((struct kernel_api * const *)0x00100000)

static inline struct kernel_api *kapi(void)
{
    return *KERNEL_API_PTR_ADDR;
}


#endif /* KERNEL_API_H */
