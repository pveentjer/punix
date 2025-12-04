#ifndef KERNEL_API_H
#define KERNEL_API_H

#include <stddef.h>
#include <stdint.h>

typedef long ssize_t;
typedef int pid_t;

struct kernel_api {
    ssize_t (*write)(int fd, const char *buf, size_t count);
    ssize_t (*read)(int fd, void *buf, size_t count);
    pid_t   (*getpid)(void);
    void    (*yield)(void);
    void    (*exit)(int status);
    void    (*sched_add_task)(const char *filename, int argc, char **argv);
};

/* Fixed address of pointer slot written by the linker header */
#define KERNEL_API_PTR_ADDR  ((struct kernel_api * const *)0x00010000)

static inline struct kernel_api *kapi(void)
{
    return *KERNEL_API_PTR_ADDR;
}

#endif /* KERNEL_API_H */
