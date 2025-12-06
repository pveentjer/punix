
#include "../../include/kernel/vfs.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"

/* ------------------------------------------------------------
 * Helper: add an entry into a struct dirent array inside user buffer
 *
 * buf_entries: pointer to start of struct dirent array
 * max_entries: how many struct dirent fit in that buffer
 * idx: pointer to current index (will be incremented on success)
 * ino: d_ino
 * d_type: d_type
 * name: NUL-terminated name to copy into d_name (truncated if necessary)
 *
 * returns 1 on success, 0 if no space
 * ------------------------------------------------------------ */
static int add_entry(struct dirent *buf_entries,
                     unsigned int max_entries,
                     unsigned int *idx,
                     uint32_t ino,
                     uint8_t d_type,
                     const char *name)
{
    if (*idx >= max_entries) return 0;

    struct dirent *de = &buf_entries[*idx];
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = d_type;

    /* safe copy and ensure NUL termination */
    if (name)
    {
        size_t nlen = k_strlen(name);
        if (nlen >= sizeof(de->d_name)) nlen = sizeof(de->d_name) - 1;
        k_memcpy(de->d_name, name, nlen);
        de->d_name[nlen] = '\0';
    }
    else
    {
        de->d_name[0] = '\0';
    }

    (*idx)++;
    return 1;
}

/* ------------------------------------------------------------
 * getdents implementation for FD_PROC, FD_ROOT, FD_BIN
 * - expects buf to be large enough for an integral number of struct dirent
 * - returns number of bytes written (idx * sizeof(struct dirent))
 * - returns -ENOSYS for unsupported fd
 * ------------------------------------------------------------ */
int vfs_getdents(int fd, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    if (fd == FD_ROOT)
    {
        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* /proc and /bin entries */
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "proc");
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "bin");

        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_PROC)
    {

        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* let scheduler fill the rest with PID dirs */
        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            int written = sched_fill_proc_dirents(&buf[idx], remaining);
            if (written > 0)
            {
                idx += (unsigned int) written;
            }
        }

        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_BIN)
    {
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            int written = elf_fill_bin_dirents(&buf[idx], remaining);
            if (written > 0)
                idx += (unsigned int) written;
        }

        return (int) (idx * sizeof(struct dirent));
    }

    return -ENOSYS;
}


int vfs_open(const char *pathname, int flags, int mode)
{
    if (!pathname) return -ENOSYS;

    if (k_strcmp(pathname, "/") == 0 || k_strcmp(pathname, "/.") == 0)
    {
        return FD_ROOT;
    }

    if (k_strcmp(pathname, "/proc") == 0 || k_strcmp(pathname, "/proc/") == 0)
    {
        return FD_PROC;
    }

    if (k_strcmp(pathname, "/bin") == 0 || k_strcmp(pathname, "/bin/") == 0)
    {
        return FD_BIN;
    }

    /* optionally support opening /proc/<pid> or /bin/<name> later */
    return -ENOSYS;
}

int vfs_close(int fd)
{
    if (fd == FD_PROC || fd == FD_ROOT || fd == FD_BIN)
    {
        return 0;
    }
    return -ENOSYS;
}