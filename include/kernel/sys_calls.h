#ifndef SYS_CALLS_H
#define SYS_CALLS_H

#include <stddef.h>
#include "dirent.h"

typedef long ssize_t;
typedef int pid_t;


enum sys_call_nr
{
    SYS_write = 0,
    SYS_read,
    SYS_open,
    SYS_close,
    SYS_getdents,
    SYS_add_task,
    SYS_fork,
    SYS_execve,
    SYS_exit,
    SYS_kill,
    SYS_waitpid,
    SYS_getpid,
    SYS_sched_yield,
    SYS_nice,
    SYS_brk,
    SYS_chdir,
    SYS_getcwd,
};

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

    uint32_t (*sys_enter_fn)(
            uint32_t nr,
            uint32_t a1,
            uint32_t a2,
            uint32_t a3,
            uint32_t a4);
};

/* 1 MiB base where the kernel header lives */
#define MB(x)               ((x) * 1024u * 1024u)
#define SYS_CALLS_HDR_ADDR  MB(1)

/* First word in the header: pointer to sys_call_instance */
#define SYS_CALLS_PTR_ADDR ((struct sys_calls * const *)SYS_CALLS_HDR_ADDR)

static inline struct sys_calls *sys(void)
{
    return *SYS_CALLS_PTR_ADDR;
}

#endif /* SYS_CALLS_H */
