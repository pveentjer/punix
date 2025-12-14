// bin_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/elf_loader.h"
#include "kernel/kutils.h"  /* k_strcmp, k_strrchr, etc */

/* find_app is provided by the ELF / embedded app layer. */
/* const struct embedded_app *find_app(const char *name); */

/* ------------------------------------------------------------------
 * /bin directory listing
 * ------------------------------------------------------------------ */

static int bin_getdents(
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

    int size = (int)(idx * sizeof(struct dirent));
    file->pos += size;
    return size;
}

int bin_open(struct file *file)
{
    if (!file || !file->pathname)
    {
        return -1;
    }

    /* Opening the /bin directory itself is allowed. */
    if ((k_strcmp(file->pathname, "/bin") == 0) ||
        (k_strcmp(file->pathname, "/bin/") == 0))
    {
        return 0;
    }

    /* Extract basename (program name) from the path. */
    const char *name = k_strrchr(file->pathname, '/');
    if (name)
    {
        name++;  /* skip '/' */
    }
    else
    {
        name = file->pathname;
    }

    if (*name == '\0')
    {
        /* Path ends with '/', but no actual name: treat as error. */
        return -1;
    }

    /* Validate that the program exists in the embedded app table. */
    const struct embedded_app *app = find_app(name);
    if (!app)
    {
        /* No such program -> open() should fail. */
        return -1;
    }

    /* If you want, you could stash something on file here
       (e.g. file->inode or a custom field) using 'app'. For now,
       just succeed and let exec/sched resolve again by name. */
    file->pos = 0;
    return 0;
}

/* ------------------------------------------------------------------
 * /bin filesystem operations
 * ------------------------------------------------------------------ */

struct fs bin_fs = {
        .open     = bin_open,
        .close    = NULL,
        .read     = NULL,
        .write    = NULL,
        .getdents = bin_getdents
};
