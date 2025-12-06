// vfs.c

#include "../../include/kernel/vfs.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/vga.h"

/* ------------------------------------------------------------
 * Temporary per-directory EOF flags
 *
 * This is a stopgap until we have a proper file descriptor
 * abstraction with per-FD position. For now:
 *  - first getdents(fd) returns all entries
 *  - subsequent getdents(fd) returns 0 (EOF)
 * ------------------------------------------------------------ */
static int root_done = 0;
static int proc_done = 0;
static int bin_done  = 0;

/* ------------------------------------------------------------
 * Helper: add an entry into a struct dirent slots inside user buffer
 *
 * buf_entries: pointer to start of struct dirent slots
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
    de->d_ino    = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type   = d_type;

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
 *
 * Linux-style semantics:
 *  - 'count' is buffer size in BYTES
 *  - return value = number of BYTES written
 *  - return 0 on EOF
 *
 * NOTE (temporary):
 *  For now we assume each directory fits in a single call.
 *  The *_done flags make subsequent calls return 0 (EOF).
 * ------------------------------------------------------------ */
int vfs_getdents(int fd, struct dirent *buf, unsigned int count)
{
    // screen_println("vfs_getdents");

    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    if (fd == FD_ROOT)
    {
        /* Already listed once? Then EOF. */
        if (root_done)
        {
            return 0;
        }

        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* /proc and /bin entries */
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "proc");
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "bin");

        root_done = 1;
        return (int)(idx * sizeof(struct dirent));
    }
    else if (fd == FD_PROC)
    {
        // screen_println("vfs_getdents: FD_PROC");

        if (proc_done)
        {
            return 0;   // EOF
        }

        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* let scheduler fill the rest with PID dirs */
        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            /*
             * sched_fill_proc_dirents:
             *  - should write up to 'remaining' entries into &buf[idx]
             *  - should return NUMBER OF ENTRIES written (not bytes)
             */
            int written = sched_fill_proc_dirents(&buf[idx], remaining);
            if (written > 0)
            {
                idx += (unsigned int)written;
            }
        }

        proc_done = 1;
        return (int)(idx * sizeof(struct dirent));
    }
    else if (fd == FD_BIN)
    {
        if (bin_done)
        {
            return 0;   // EOF
        }

        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            /*
             * elf_fill_bin_dirents:
             *  - should write up to 'remaining' entries into &buf[idx]
             *  - should return NUMBER OF ENTRIES written (not bytes)
             */
            int written = elf_fill_bin_dirents(&buf[idx], remaining);
            if (written > 0)
            {
                idx += (unsigned int)written;
            }
        }

        bin_done = 1;
        return (int)(idx * sizeof(struct dirent));
    }

    return -ENOSYS;
}

/* ------------------------------------------------------------
 * vfs_open
 *
 * Very simple dispatcher that returns magic FDs for /, /proc, /bin.
 * Later this should be replaced by a real VFS lookup + file table.
 * ------------------------------------------------------------ */
int vfs_open(const char *pathname, int flags, int mode)
{
    (void)flags;
    (void)mode;

    if (!pathname)
    {
        return -ENOSYS;
    }

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

/* ------------------------------------------------------------
 * vfs_close
 *
 * Resets the per-dir EOF flags for our magic FDs.
 * ------------------------------------------------------------ */
int vfs_close(int fd)
{
    if (fd == FD_ROOT)
    {
        root_done = 0;
        return 0;
    }

    if (fd == FD_PROC)
    {
        proc_done = 0;
        return 0;
    }

    if (fd == FD_BIN)
    {
        bin_done = 0;
        return 0;
    }

    return -ENOSYS;
}
