#ifndef TYPES_H
#define TYPES_H

#include "stddef.h"

/* -------- POSIX-ish base types -------- */
typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef unsigned long mode_t;
typedef unsigned long nlink_t;
typedef unsigned long uid_t;
typedef unsigned long gid_t;
typedef long          off_t;
typedef long          blkcnt_t;
typedef long          blksize_t;

/* -------- Common nonstandard helpers -------- */
typedef long ssize_t;

typedef long time_t;
typedef int pid_t;

#endif /* TYPES_H */
