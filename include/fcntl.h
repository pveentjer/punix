#ifndef FCNTL_H
#define FCNTL_H

#include "sys/types.h"

// POSIX-style fd numbers
#define STDIN_FILENO   0
#define STDOUT_FILENO  1
#define STDERR_FILENO  2

// open() flags (minimal subset)
#define O_RDONLY   0x0
#define O_WRONLY   0x1
#define O_RDWR     0x2
#define O_CREAT    0x40
#define O_TRUNC    0x200


ssize_t write(int fd, const void *buf, size_t count);

ssize_t read(int fd, void *buf, size_t count);

int open(const char *pathname, int flags, int mode);

int close(int fd);


#endif //FCNTL_H
