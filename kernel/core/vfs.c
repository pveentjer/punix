// vfs.c

#include "../../include/kernel/vfs.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/constants.h"

#define VFS_RING_MASK (RLIMIT_NOFILE - 1)

// ==================== files =======================================

// ==================== vfs =======================================

struct vfs vfs;  // this creates the actual storage

void vfs_init(
        struct vfs *vfs)
{
    vfs->free_head = 0;
    vfs->free_tail = MAX_FILE_CNT;

    for (int file_idx = 0; file_idx < MAX_FILE_CNT; file_idx++)
    {
        struct file *file = &vfs->files[file_idx];

        k_memset(file, 0, sizeof(struct file));
        file->idx = file_idx;
        vfs->free_ring[file_idx] = file_idx;
    }
}

static int to_fs_type(
        const char *pathname)
{
    if (!pathname)
    {
        return -1;
    }

    // Must start with '/'
    if (pathname[0] != '/')
    {
        return -1;
    }

    const char *p = pathname + 1;  // skip leading '/'
    const char *s = p;

    // Find end of first component or end of string
    while (*s != '\0' && *s != '/')
    {
        s++;
    }

    size_t len = (size_t) (s - p);

    // Cases:
    // "/"           -> p == "", len == 0         -> root
    // "/."          -> p == ".", len == 1        -> root
    // "/foo"        -> p == "foo", len == 3      -> maybe special
    // "/foo/bar"    -> p == "foo", len == 3      -> same FS as "/foo"

    if (len == 0)
    {
        // "/" or "/<nothing>" -> root FS
        return FD_ROOT;
    }

    if (len == 1 && p[0] == '.')
    {
        // "/." -> root FS
        return FD_ROOT;
    }

    if (len == 4 && k_strncmp(p, "proc", 4) == 0)
    {
        return FD_PROC;
    }

    if (len == 3 && k_strncmp(p, "bin", 3) == 0)
    {
        return FD_BIN;
    }

    if (len == 3 && k_strncmp(p, "dev", 3) == 0)
    {
        return FD_DEV;
    }

    // Anything else lives on the root filesystem: /etc, /home, /foo/bar, ...
    return FD_ROOT;
}


/* ------------------------------------------------------------
 * vfs_open
 *
 * Very simple dispatcher that returns magic FDs for /, /proc, /bin.
 * Later this should be replaced by a real VFS lookup + file table.
 * ------------------------------------------------------------ */
int vfs_open(
        struct vfs *vfs,
        struct task *task,
        const char *pathname,
        int flags,
        int mode)
{
    int vfs_type = to_fs_type(pathname);
    if (vfs_type == -1)
    {
        return -1;
    }

    if (task == NULL)
    {
        return -1;
    }

    if (vfs->free_head == vfs->free_tail)
    {
        // no space
        return -1;
    }

    const uint32_t free_ring_idx = vfs->free_head & VFS_RING_MASK;
    const uint32_t file_idx = vfs->free_ring[free_ring_idx];

    struct file *file = &vfs->files[file_idx];

    vfs->free_head++;

    int fd = files_alloc_fd(&task->files, file);
    if (fd < 0)
    {
        //todo
        //vfs_close(file);
        return -1;
    }

    file->done = false;
    file->flags = flags;
    file->mode = mode;
    file->fs_type = vfs_type;
    k_strcpy(file->pathname, pathname);
    return fd;
}


int vfs_close(
        struct vfs *vfs,
        struct task *task,
        const int fd)
{
    if (fd < 0)
    {
        return -1;
    }

    if (task == NULL)
    {
        return -1;
    }

    if (vfs->free_tail - vfs->free_head == MAX_FILE_CNT)
    {
        panic("vfs_file_free: too many frees.");
    }

    struct file *file = files_free_fd(&task->files, fd);
    if (file == NULL)
    {
        return -1;
    }

    const uint32_t file_idx = file->idx;
    const uint32_t free_ring_idx = vfs->free_tail & VFS_RING_MASK;

    vfs->free_ring[free_ring_idx] = file_idx;
    vfs->free_tail++;
    return 0;
}


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
static int add_entry(
        struct dirent *buf_entries,
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
int vfs_getdents(
        struct vfs *vfs,
        struct task *task,
        int fd,
        struct dirent *buf,
        unsigned int count)
{
    // screen_println("vfs_getdents");

    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        panic("vfs_close: current is NULL");
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL)
    {
        return -1;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    if (fd == FD_ROOT)
    {
        /* Already listed once? Then EOF. */
        if (file->done)
        {
            return 0;
        }

        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* /proc and /bin entries */
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "proc");
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "bin");

        file->done;
        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_PROC)
    {
        // screen_println("vfs_getdents: FD_PROC");
        if (file->done)
        {
            return 0;
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
                idx += (unsigned int) written;
            }
        }

        file->done;
        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_BIN)
    {
        if (file->done)
        {
            return 0;
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
                idx += (unsigned int) written;
            }
        }

        file->done;
        return (int) (idx * sizeof(struct dirent));
    }

    return -ENOSYS;
}


