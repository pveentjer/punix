// bin_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/elf.h"

static int bin_getdents(struct file *file, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    if (file->done)
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

    if (idx < max_entries)
    {
        unsigned int remaining = max_entries - idx;
        int written = elf_fill_bin_dirents(&buf[idx], remaining);
        if (written > 0)
        {
            idx += (unsigned int) written;
        }
    }

    file->done = true;
    return (int) (idx * sizeof(struct dirent));
}

struct fs bin_fs = {
        .open = NULL,
        .close = NULL,
        .read = NULL,
        .write = NULL,
        .getdents = bin_getdents
};