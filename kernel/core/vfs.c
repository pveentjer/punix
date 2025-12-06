// vfs.c

#include "../../include/kernel/vfs.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/elf.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/constants.h"
#include "../../include/kernel/limts.h"

#define FD_MASK     (MAX_FILE_CNT - 1)

#define UNUSED_FD   -1

#define FD_MAX      INT_MAX

#define MAX_GENERATION   (FD_MAX / MAX_FILE_CNT)


void vfs_table_init(struct vfs_table *vfs_table)
{
    vfs_table->free_head = 0;
    vfs_table->free_tail = MAX_FILE_CNT;

    for (int slot_idx = 0; slot_idx < MAX_FILE_CNT; slot_idx++)
    {
        struct vfs_slot *slot = &vfs_table->slots[slot_idx];
        slot->generation = 0;   // or slot->round if that's your field name

        struct file *file = &slot->file;
        k_memset(file, 0, sizeof(struct file));
        file->fd = UNUSED_FD;

        vfs_table->free_ring[slot_idx] = slot_idx;
    }
}

struct file *vfs_file_find_by_fd(
        const struct vfs_table *vfs_table,
        const int fd)
{
    if (fd < 0)
    {
        return NULL;
    }

    uint32_t slot_idx = fd & FD_MASK;
    struct file *file = &vfs_table->slots[slot_idx].file;
    if (file->fd != fd)
    {
        return NULL;
    }
    else
    {
        return file;
    }
}

struct file *vfs_file_alloc(struct vfs_table *vfs_table)
{
    if (vfs_table->free_head == vfs_table->free_tail)
    {
        return NULL;
    }

    const uint32_t free_ring_idx = vfs_table->free_head & FD_MASK;
    const uint32_t slot_idx      = vfs_table->free_ring[free_ring_idx];

    struct vfs_slot *slot = &vfs_table->slots[slot_idx];
    struct file *file     = &slot->file;

    file->fd = slot->generation * MAX_FILE_CNT + slot_idx;
    // we need to take care of the wrap around of the slot->generation
    slot->generation = (slot->generation + 1) & MAX_GENERATION;
    vfs_table->free_head++;

    return file;
}

void vfs_file_free(
        struct vfs_table *vfs_table,
        struct file *file)
{
    if (vfs_table->free_tail - vfs_table->free_head == MAX_FILE_CNT)
    {
        panic("vfs_file_free: too many frees.");
    }

    const int fd = file->fd;
    if (fd < 0)
    {
        panic("vfs_file_free: file has negative fd.");
    }

    const uint32_t slot_idx      = fd & FD_MASK;
    const uint32_t free_ring_idx = vfs_table->free_tail & FD_MASK;

    struct vfs_slot *slot = &vfs_table->slots[slot_idx];
    if (&slot->file != file)
    {
        panic("vfs_file_free: file pointer/slot mismatch");
    }

    vfs_table->free_ring[free_ring_idx] = slot_idx;
    vfs_table->free_tail++;
    file->fd = UNUSED_FD;
}


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
