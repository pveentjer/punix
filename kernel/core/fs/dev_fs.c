// dev_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"
#include "kernel/vga.h"
#include "kernel/keyboard.h"
#include "kernel/constants.h"

static ssize_t dev_read(struct file *file, void *buf, size_t count)
{
    if (!buf || count == 0)
    {
        return 0;
    }

    const char *name = k_strrchr(file->pathname, '/');
    if (!name)
    {
        name = file->pathname;
    }
    else
    {
        name++;
    }

    if (k_strcmp(name, "stdin") != 0)
    {
        return -1;
    }

    return keyboard_read((char *) buf, count);
}

static ssize_t dev_write(struct file *file, const void *buf, size_t count)
{
    if (!buf || count == 0)
    {
        return 0;
    }

    const char *name = k_strrchr(file->pathname, '/');
    if (!name) name = file->pathname;
    else name++;

    if (k_strcmp(name, "stdout") != 0 && k_strcmp(name, "stderr") != 0)
    {
        return -1;
    }

    const char *cbuf = (const char *) buf;
    for (size_t i = 0; i < count; i++)
    {
        screen_put_char(cbuf[i]);
    }

    return (ssize_t) count;
}

static int dev_getdents(struct file *file, struct dirent *buf, unsigned int count)
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
    fs_add_entry(buf, max_entries, &idx, FD_STDIN, DT_CHR, "stdin");
    fs_add_entry(buf, max_entries, &idx, FD_STDOUT, DT_CHR, "stdout");
    fs_add_entry(buf, max_entries, &idx, FD_STDERR, DT_CHR, "stderr");

    int size = (int) (idx * sizeof(struct dirent));
    file->pos += size;
    return size;
}

struct fs dev_fs = {
        .open = NULL,
        .close = NULL,
        .read = dev_read,
        .write = dev_write,
        .getdents = dev_getdents
};