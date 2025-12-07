#ifndef FS_UTIL_H
#define FS_UTIL_H

#include "dirent.h"
#include "files.h"

/* Helper: add an entry into a struct dirent buffer */
int fs_add_entry(
        struct dirent *buf_entries,
        unsigned int max_entries,
        unsigned int *idx,
        uint32_t ino,
        uint8_t d_type,
        const char *name);

#endif // FS_UTIL_H