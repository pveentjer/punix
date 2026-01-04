#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>
#include "dirent.h"
#include "syscall_arch.h"


#define SYS_exit            1
#define SYS_fork            2
#define SYS_read            3
#define SYS_write           4
#define SYS_open            5
#define SYS_close           6
#define SYS_waitpid         7
#define SYS_execve          11
#define SYS_chdir           12
#define SYS_getpid          20
#define SYS_nice            34
#define SYS_kill            37
#define SYS_brk             45
#define SYS_getdents        141
#define SYS_sched_yield     158
#define SYS_getcwd          183
#define SYS_clock_gettime   265

// Custom syscalls (no Linux equivalent)
#define SYS_setctty   500 // Linux uses ioctl(fd, TIOCSCTTY, 0)


#endif /* SYSCALL_H */
