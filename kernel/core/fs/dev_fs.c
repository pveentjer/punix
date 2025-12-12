// dev_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"
#include "kernel/console.h"
#include "kernel/tty.h"
#include "kernel/constants.h"
#include "kernel/sched.h"

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

    return tty_write(file->tty, (const char *) buf, count);
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

    /* Only one page of entries; subsequent calls return 0. */
    if (file->pos > 0)
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 2, DT_DIR, "..");

    fs_add_entry(buf, max_entries, &idx, FD_STDIN, DT_CHR, "stdin");
    fs_add_entry(buf, max_entries, &idx, FD_STDOUT, DT_CHR, "stdout");
    fs_add_entry(buf, max_entries, &idx, FD_STDERR, DT_CHR, "stderr");

    for (size_t i = 0; i < TTY_COUNT; i++)
    {
        if (idx >= max_entries)
        {
            break;
        }

        char num_buf[8];
        char name_buf[16];

        k_strcpy(name_buf, "tty");
        k_itoa((int) i, num_buf);
        k_strcat(name_buf, num_buf);

        fs_add_entry(buf, max_entries, &idx, (int) (100 + i), DT_CHR, name_buf);
    }

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

    if ((k_strcmp(file->pathname, "/dev") == 0) ||
        (k_strcmp(file->pathname, "/dev/") == 0))
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

    /* NEEDED CHANGE: bind std* to the task's controlling TTY (ctty) */
    if ((k_strcmp(name, "stdin") == 0) ||
        (k_strcmp(name, "stdout") == 0) ||
        (k_strcmp(name, "stderr") == 0))
    {
        struct task *curr = sched_current();

        if (curr && curr->ctty)
        {
            file->tty = curr->ctty;
        }
        else
        {
            file->tty = tty_active();  // fallback (e.g. early boot)
        }

        return 0;
    }

    if (k_strncmp(name, "tty", 3) == 0)
    {
        const char *p = name + 3;
        if (*p == '\0')
        {
            return -1;
        }

        int idx = 0;
        while (*p)
        {
            if (*p < '0' || *p > '9')
            {
                return -1;
            }
            idx = idx * 10 + (*p - '0');
            p++;
        }

        if (idx < 0 || (size_t) idx >= TTY_COUNT)
        {
            return -1;
        }

        file->tty = tty_get((size_t) idx);
        return 0;
    }

    return -1;
}

struct fs dev_fs = {
        .open     = dev_open,
        .close    = NULL,
        .read     = dev_read,
        .write    = dev_write,
        .getdents = dev_getdents
};
