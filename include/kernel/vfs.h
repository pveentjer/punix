#ifndef VFS_H
#define VFS_H

#include "../../include/kernel/dirent.h"


/* Fake fds for virtual filesystems */
#define FD_PROC    4
#define FD_ROOT    5
#define FD_BIN     6

int vfs_getdents(int fd, struct dirent *buf, unsigned int count);

int vfs_open(const char *pathname, int flags, int mode);

int vfs_close(int fd);

#endif //VFS_H
