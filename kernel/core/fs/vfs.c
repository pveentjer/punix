// vfs.c

#include "kernel/vfs.h"
#include "kernel/kutils.h"
#include "kernel/vga.h"

#define VFS_RING_MASK (MAX_FILE_CNT - 1)

// External filesystem declarations
extern struct fs root_fs;
extern struct fs proc_fs;
extern struct fs bin_fs;
extern struct fs dev_fs;

// ==================== VFS =======================================

struct vfs vfs;

void vfs_init(struct vfs *vfs)
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

static struct fs *path_to_fs(const char *pathname)
{
    if (!pathname || pathname[0] != '/')
    {
        return NULL;
    }

    const char *p = pathname + 1;
    const char *s = p;

    while (*s != '\0' && *s != '/')
    {
        s++;
    }

    size_t len = (size_t) (s - p);

    if (len == 0 || (len == 1 && p[0] == '.'))
    {
        return &root_fs;
    }

    if (len == 4 && k_strncmp(p, "proc", 4) == 0)
    {
        return &proc_fs;
    }

    if (len == 3 && k_strncmp(p, "bin", 3) == 0)
    {
        return &bin_fs;
    }

    if (len == 3 && k_strncmp(p, "dev", 3) == 0)
    {
        return &dev_fs;
    }

    return &root_fs;
}

int vfs_open(
        struct vfs *vfs,
        struct task *task,
        const char *pathname,
        int flags,
        int mode)
{
    struct fs *fs = path_to_fs(pathname);
    if (fs == NULL)
    {
        return -1;
    }

    if (task == NULL)
    {
        return -1;
    }

    if (vfs->free_head == vfs->free_tail)
    {
        return -1;
    }

    const uint32_t free_ring_idx = vfs->free_head & VFS_RING_MASK;
    const uint32_t file_idx = vfs->free_ring[free_ring_idx];

    struct file *file = &vfs->files[file_idx];

    vfs->free_head++;

    int fd = files_alloc_fd(&task->files, file);
    if (fd < 0)
    {
        // TODO: rollback vfs allocation
        return -1;
    }

    file->fd = fd;
    file->done = false;
    file->flags = flags;
    file->mode = mode;
    file->fs = fs;
    k_strcpy(file->pathname, pathname);

    // Call filesystem-specific open if it exists
    if (fs->open != NULL)
    {
        int result = fs->open(file);
        if (result < 0)
        {
            files_free_fd(&task->files, fd);
            return -1;
        }
    }

    return fd;
}

int vfs_close(
        struct vfs *vfs,
        struct task *task,
        const int fd)
{
    if (fd < 0 || task == NULL)
    {
        return -1;
    }

    if (vfs->free_tail - vfs->free_head == MAX_FILE_CNT)
    {
        panic("vfs_close: too many frees");
    }

    struct file *file = files_find_by_fd(&task->files, fd);
    if (file == NULL)
    {
        return -1;
    }

    // Call filesystem-specific close if it exists
    if (file->fs != NULL && file->fs->close != NULL)
    {
        file->fs->close(file);
    }

    file->fd = -1;

    files_free_fd(&task->files, fd);

    const uint32_t file_idx = file->idx;
    const uint32_t free_ring_idx = vfs->free_tail & VFS_RING_MASK;

    vfs->free_ring[free_ring_idx] = file_idx;
    vfs->free_tail++;

    return 0;
}