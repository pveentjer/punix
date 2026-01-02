// sys_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"

static int sys_open(struct file *file)
{
    if (!file || !file->pathname)
    {
        return -1;
    }

    // Must start with /sys
    if (k_strncmp(file->pathname, "/sys", 4) != 0)
    {
        return -1;
    }

    // Opening /sys or /sys/ itself (directory listing)
    if (file->pathname[4] == '\0' ||
        (file->pathname[4] == '/' && file->pathname[5] == '\0'))
    {
        return 0;  // Allow directory operations
    }

    // No subdirectories/files yet
    return -1;
}

static int sys_close(struct file *file)
{
    return 0;
}

static ssize_t sys_read(struct file *file, void *buf, size_t count)
{
    return -1;  // No readable files yet
}

static ssize_t sys_write(struct file *file, const void *buf, size_t count)
{
    return -1;  // No writable files yet
}

static int sys_getdents(struct file *file, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    if (file->pos > 0)
    {
        return 0;  // Already read
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 2, DT_DIR, "..");

    // Empty for now - add sys entries here later

    int size = (int)(idx * sizeof(struct dirent));
    file->pos += size;
    return size;
}

struct fs sys_fs = {
        .open     = sys_open,
        .close    = sys_close,
        .read     = sys_read,
        .write    = sys_write,
        .getdents = sys_getdents
};