// root_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/vfs.h"
#include "dirent.h"


static int root_getdents(struct file *file, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    if (file->pos > 0)
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

    vfs_get_root_mounts(buf, max_entries, &idx);

    int size = (int)(idx * sizeof(struct dirent));
    file->pos += size;
    return size;
}

int root_open(struct file *file)
{
    file->file_ops.getdents = root_getdents;
    return 0;
}

struct fs root_fs = {
        .open = root_open,
};