// root_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/dirent.h"

static int root_getdents(struct file *file, struct dirent *buf, unsigned int count)
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
    fs_add_entry(buf, max_entries, &idx, 0, DT_DIR, "proc");
    fs_add_entry(buf, max_entries, &idx, 0, DT_DIR, "bin");
    fs_add_entry(buf, max_entries, &idx, 0, DT_DIR, "dev");

    file->done = true;
    return (int) (idx * sizeof(struct dirent));
}

struct fs root_fs = {
        .open = NULL,
        .close = NULL,
        .read = NULL,
        .write = NULL,
        .getdents = root_getdents
};