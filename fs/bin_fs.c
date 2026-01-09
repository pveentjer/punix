// bin_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/elf_loader.h"
#include "kernel/kutils.h"

/* find_bin is provided by the ELF / embedded app layer. */
/* const struct embedded_bin *find_bin(const char *name); */

/* ------------------------------------------------------------
 * Helper to lookup bin by pathname
 * ------------------------------------------------------------ */

static int bin_lookup(const char *pathname, const struct embedded_bin **out_bin)
{
    if (!pathname || *pathname == '\0')
    {
        return -1;
    }

    const struct embedded_bin *bin = find_bin(pathname);
    if (!bin)
    {
        return -1;
    }

    *out_bin = bin;
    return 0;
}

/* ------------------------------------------------------------
 * Fill dirent entries for /bin from embedded_bins
 * ------------------------------------------------------------ */

static void fill_dirent_from_bin(struct dirent *de,
                                 const struct embedded_bin *bin,
                                 uint32_t ino)
{
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = DT_REG;

    /* Strip any leading path: "/bin/ls" -> "ls" */
    const char *name = bin->name;
    const char *base = name;

    const char *p = k_strrchr(name, '/');
    if (p && p[1] != '\0')
    {
        base = p + 1;
    }

    size_t len = k_strlen(base);
    if (len >= sizeof(de->d_name))
    {
        len = sizeof(de->d_name) - 1;
    }

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
        fill_dirent_from_bin(&buf[idx], &embedded_bins[i],
                             (uint32_t) (i + 1));  // fake inode
        idx++;
    }

    return (int) idx;
}

/* ------------------------------------------------------------------
 * /bin file operations
 * ------------------------------------------------------------------ */

static ssize_t bin_read(struct file *file, void *buf, size_t count)
{
    if (!file || !buf)
    {
        return -1;
    }

    const struct embedded_bin *bin = (const struct embedded_bin *) file->driver_data;
    if (!bin)
    {
        /* Reading from directory or invalid file */
        return -1;
    }

    size_t size = (size_t) (bin->end - bin->start);

    /* Check if we're at or past EOF */
    if (file->pos >= size)
    {
        return 0;  /* EOF */
    }

    /* Calculate how much to read */
    size_t remaining = size - file->pos;
    size_t to_read = (count < remaining) ? count : remaining;

    /* Copy data */
    k_memcpy(buf, bin->start + file->pos, to_read);
    file->pos += to_read;

    return (ssize_t) to_read;
}

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
    file->file_ops.read = bin_read;
    file->file_ops.getdents = bin_getdents;

    /* Opening the /bin directory itself is allowed. */
    if ((k_strcmp(file->pathname, "/bin") == 0) ||
        (k_strcmp(file->pathname, "/bin/") == 0))
    {
        return 0;
    }

    const struct embedded_bin *bin = NULL;
    if (bin_lookup(file->pathname, &bin) < 0)
    {
        /* No such program -> open() should fail. */
        return -1;
    }

    file->driver_data = (void *) bin;
    return 0;
}


/* ------------------------------------------------------------------
 * /bin filesystem operations
 * ------------------------------------------------------------------ */

struct fs bin_fs = {
        .open     = bin_open,
};