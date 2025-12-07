// fs_util.c

#include "kernel/fs_util.h"
#include "kernel/kutils.h"

int fs_add_entry(
        struct dirent *buf_entries,
        unsigned int max_entries,
        unsigned int *idx,
        uint32_t ino,
        uint8_t d_type,
        const char *name)
{
    if (*idx >= max_entries) return 0;

    struct dirent *de = &buf_entries[*idx];
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = d_type;

    if (name)
    {
        size_t nlen = k_strlen(name);
        if (nlen >= sizeof(de->d_name)) nlen = sizeof(de->d_name) - 1;
        k_memcpy(de->d_name, name, nlen);
        de->d_name[nlen] = '\0';
    }
    else
    {
        de->d_name[0] = '\0';
    }

    (*idx)++;
    return 1;
}