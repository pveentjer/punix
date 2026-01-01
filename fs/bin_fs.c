// bin_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/elf_loader.h"
#include "kernel/kutils.h"

/* find_app is provided by the ELF / embedded app layer. */
/* const struct embedded_bin *find_app(const char *name); */



/* ------------------------------------------------------------
 * Fill dirent entries for /bin from embedded_bins
 * ------------------------------------------------------------ */

static void fill_dirent_from_app(struct dirent *de,
                                 const struct embedded_bin *app,
                                 uint32_t ino)
{
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = DT_REG;

    /* Strip any leading path: "/bin/ls" -> "ls" */
    const char *name = app->name;
    const char *base = name;

    const char *p = k_strrchr(name, '/');
    if (p && p[1] != '\0')
        base = p + 1;

    size_t len = k_strlen(base);
    if (len >= sizeof(de->d_name))
        len = sizeof(de->d_name) - 1;

    k_memcpy(de->d_name, base, len);
    de->d_name[len] = '\0';
}

int elf_fill_bin_dirents(struct dirent *buf, unsigned int max_entries)
{
    unsigned int idx = 0;

    if (!buf || max_entries == 0)
    {
        return 0;
    }

    for (size_t i = 0; i < embedded_bin_count && idx < max_entries; ++i)
    {
        fill_dirent_from_app(&buf[idx], &embedded_bins[i],
                             (uint32_t) (i + 1));  // fake inode
        idx++;
    }

    return (int) idx;
}


/* ------------------------------------------------------------------
 * /bin directory listing
 * ------------------------------------------------------------------ */

static int bin_getdents(struct file *file, struct dirent *buf, unsigned int count)
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

    int size = (int) (idx * sizeof(struct dirent));
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
    const struct embedded_bin *app = find_app(name);
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
