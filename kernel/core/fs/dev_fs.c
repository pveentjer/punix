// dev_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"
#include "kernel/console.h"
#include "kernel/tty.h"
#include "kernel/constants.h"

static ssize_t dev_read(
        struct file *file,
        void *buf, size_t count)
{
    if ((file == NULL) ||
        (file->tty == NULL) ||
        (buf == NULL) ||
        (count == 0))
    {
        return 0;
    }

    return tty_read(file->tty, (char *) buf, count);
}

static ssize_t dev_write(
        struct file *file,
        const void *buf, size_t count)
{
    if ((file == NULL) ||
        (file->tty == NULL) ||
        (buf == NULL) ||
        (count == 0))
    {
        return 0;
    }

    return tty_write(file->tty, (char *) buf, count);
}

static int dev_getdents(
        struct file *file,
        struct dirent *buf,
        unsigned int count)
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

int dev_open(
        struct file *file)
{
    if ((file == NULL) || (file->pathname == NULL))
    {
        return -1;
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

    if ((k_strcmp(name, "stdin") == 0) ||
        (k_strcmp(name, "stdout") == 0) ||
        (k_strcmp(name, "stderr") == 0))
    {
        file->tty = tty_active();
    }


    return 0;
}

struct fs dev_fs = {
        .open     = dev_open,
        .close    = NULL,
        .read     = dev_read,
        .write    = dev_write,
        .getdents = dev_getdents
};
