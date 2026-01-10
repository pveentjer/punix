#ifndef _SYS_STAT_H
#define _SYS_STAT_H

#include "sys/types.h"
#include "time.h"

#define S_IFMT   0170000   /* type bit mask */
#define S_IFREG  0100000   /* regular file   */
#define S_IFDIR  0040000   /* directory      */

struct stat {
    dev_t     st_dev;
    ino_t     st_ino;
    mode_t    st_mode;
    nlink_t   st_nlink;
    uid_t     st_uid;
    gid_t     st_gid;
    dev_t     st_rdev;
    off_t     st_size;
    blksize_t st_blksize;
    blkcnt_t  st_blocks;

    struct timespec st_atim;
    struct timespec st_mtim;
    struct timespec st_ctim;
};



int stat(const char *pathname, struct stat *buf);

int fstat(int fd, struct stat *buf);

int lstat(const char *pathname, struct stat *buf);

#endif /* _SYS_STAT_H */
