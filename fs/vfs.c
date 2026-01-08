// vfs.c

#include "errno.h"
#include "kernel/vfs.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"
#include "kernel/console.h"

#define VFS_RING_MASK     (MAX_FILE_CNT - 1)
#define VFS_MAX_SEGMENTS  64
#define MAX_MOUNTS        16



struct mount_point {
    char path[MAX_FILENAME_LEN];
    struct fs *fs;
    int active;
};

struct vfs {
    uint32_t free_ring[MAX_FILE_CNT];
    uint32_t free_head;
    uint32_t free_tail;
    struct file files[MAX_FILE_CNT];
    struct mount_point mounts[MAX_MOUNTS];
};

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

    // Initialize mount table
    for (int i = 0; i < MAX_MOUNTS; i++)
    {
        vfs->mounts[i].active = 0;
        vfs->mounts[i].fs = NULL;
        vfs->mounts[i].path[0] = '\0';
    }
}

/* ------------------------------------------------------------
 * Path normalization
 *
 * Input: absolute path or best-effort string in 'path'
 * Output: normalized absolute path in-place:
 *   - ensures leading '/'
 *   - collapses multiple '/'
 *   - resolves '.' and '..'
 *   - strips trailing '/' except for root
 * ------------------------------------------------------------ */
static void vfs_normalize_path(char *path, size_t buf_size)
{
    if (!path || buf_size == 0)
    {
        return;
    }

    if (path[0] == '\0')
    {
        path[0] = '/';
        if (buf_size > 1)
        {
            path[1] = '\0';
        }
        return;
    }

    /* Ensure leading '/' */
    if (path[0] != '/')
    {
        size_t len = k_strlen(path);
        if (len + 1 >= buf_size)
        {
            if (buf_size < 2)
            {
                return;
            }
            len = buf_size - 2;
        }

        /* Shift right by one, including '\0' */
        for (size_t i = len + 1; i > 0; i--)
        {
            path[i] = path[i - 1];
        }
        path[0] = '/';
        /* string already null-terminated by the shift */
    }

    struct segment
    {
        int start;
        int len;
    };

    struct segment segs[VFS_MAX_SEGMENTS];
    int seg_count = 0;

    size_t len = k_strlen(path);
    int i = 0;

    while (i < (int) len)
    {
        /* Skip consecutive '/' */
        while (i < (int) len && path[i] == '/')
        {
            i++;
        }

        int start = i;
        while (i < (int) len && path[i] != '/')
        {
            i++;
        }

        int comp_len = i - start;
        if (comp_len == 0)
        {
            continue;
        }

        /* Single '.' -> skip */
        if (comp_len == 1 && path[start] == '.')
        {
            continue;
        }

        /* '..' -> pop previous segment if any */
        if (comp_len == 2 && path[start] == '.' && path[start + 1] == '.')
        {
            if (seg_count > 0)
            {
                seg_count--;
            }
            /* if at root, stay at root */
            continue;
        }

        if (seg_count < VFS_MAX_SEGMENTS)
        {
            segs[seg_count].start = start;
            segs[seg_count].len = comp_len;
            seg_count++;
        }
    }

    /* Rebuild normalized path */
    int pos = 0;

    if (seg_count == 0)
    {
        /* Just root */
        path[0] = '/';
        if (buf_size > 1)
        {
            path[1] = '\0';
        }
        return;
    }

    path[pos++] = '/';

    for (int s = 0; s < seg_count; s++)
    {
        if (pos + segs[s].len + 1 >= (int) buf_size)
        {
            int allowed = (int) buf_size - pos - 2;
            if (allowed <= 0)
            {
                break;
            }
            segs[s].len = allowed;
        }

        k_memcpy(path + pos, path + segs[s].start, (size_t) segs[s].len);
        pos += segs[s].len;

        if (s != seg_count - 1)
        {
            if (pos + 1 >= (int) buf_size)
            {
                break;
            }
            path[pos++] = '/';
        }
    }

    if (pos >= (int) buf_size)
    {
        pos = (int) buf_size - 1;
    }
    path[pos] = '\0';
}

/* ------------------------------------------------------------
 * Mount a filesystem at a path
 * ------------------------------------------------------------ */
int vfs_mount(const char *path, struct fs *fs)
{
    if (!path || !fs)
    {
        return -1;
    }

    // Normalize the mount path
    char normalized[MAX_FILENAME_LEN];
    k_strncpy(normalized, path, sizeof(normalized));
    normalized[sizeof(normalized) - 1] = '\0';
    vfs_normalize_path(normalized, sizeof(normalized));

    // Find free slot
    for (int i = 0; i < MAX_MOUNTS; i++)
    {
        if (!vfs.mounts[i].active)
        {
            vfs.mounts[i].active = 1;
            vfs.mounts[i].fs = fs;
            k_strcpy(vfs.mounts[i].path, normalized);
            return 0;
        }
    }

    return -1; // No free slots
}

void vfs_get_root_mounts(struct dirent *buf, unsigned int max_entries, unsigned int *idx)
{
    for (int i = 0; i < MAX_MOUNTS; i++)
    {
        if (*idx >= max_entries)
        {
            break;
        }

        if (!vfs.mounts[i].active)
        {
            continue;
        }

        const char *path = vfs.mounts[i].path;

        // Skip root mount itself
        if (path[0] == '/' && path[1] == '\0')
        {
            continue;
        }

        // Must start with /
        if (path[0] != '/')
        {
            continue;
        }

        // Extract name after /
        const char *name = path + 1;

        // Only add if it's a direct child of root (no more slashes)
        if (k_strchr(name, '/') == NULL && name[0] != '\0')
        {
            fs_add_entry(buf, max_entries, idx, 0, DT_DIR, name);
        }
    }
}

/* ------------------------------------------------------------
 * Find filesystem for a path using mount table
 * Returns the filesystem with the longest matching prefix
 * ------------------------------------------------------------ */
static struct fs *path_to_fs(const char *pathname)
{
    if (!pathname || pathname[0] != '/')
    {
        return NULL;
    }

    struct fs *best_fs = NULL;
    size_t best_len = 0;

    // Find longest matching mount point
    for (int i = 0; i < MAX_MOUNTS; i++)
    {
        if (!vfs.mounts[i].active)
        {
            continue;
        }

        const char *mount_path = vfs.mounts[i].path;
        size_t mount_len = k_strlen(mount_path);

        // Check if pathname starts with this mount path
        if (k_strncmp(pathname, mount_path, mount_len) == 0)
        {
            // Exact match or followed by '/' or end of string
            if (pathname[mount_len] == '\0' || pathname[mount_len] == '/' || mount_len == 1)
            {
                if (mount_len > best_len)
                {
                    best_len = mount_len;
                    best_fs = vfs.mounts[i].fs;
                }
            }
        }
    }

    return best_fs;
}

struct file *vfs_alloc_file()
{
    if (vfs.free_head == vfs.free_tail)
    {
        return NULL;
    }

    const uint32_t free_ring_idx = vfs.free_head & VFS_RING_MASK;
    const uint32_t file_idx = vfs.free_ring[free_ring_idx];

    struct file *file = &vfs.files[file_idx];

    vfs.free_head++;
    return file;
}

void vfs_free_file(struct file *file)
{
    if (vfs.free_tail - vfs.free_head == MAX_FILE_CNT)
    {
        panic("vfs_free_file: too many frees");
    }

    file->fd = -1;

    const uint32_t file_idx = file->idx;
    const uint32_t free_ring_idx = vfs.free_tail & VFS_RING_MASK;

    vfs.free_ring[free_ring_idx] = file_idx;
    vfs.free_tail++;
}

/* ------------------------------------------------------------
 * Path resolution:
 *   - combine task->cwd and pathname
 *   - normalize the resulting absolute path
 * ------------------------------------------------------------ */
void vfs_resolve_path(const char *cwd, const char *pathname, char *resolved, size_t resolved_size)
{
    if (!resolved || resolved_size == 0)
    {
        return;
    }

    /* Empty path -> cwd or "/" */
    if (!pathname || pathname[0] == '\0')
    {
        if (cwd[0] != '\0')
        {
            k_strncpy(resolved, cwd, resolved_size);
            resolved[resolved_size - 1] = '\0';
        }
        else
        {
            resolved[0] = '/';
            if (resolved_size > 1)
            {
                resolved[1] = '\0';
            }
        }
        vfs_normalize_path(resolved, resolved_size);
        return;
    }

    /* Already absolute */
    if (pathname[0] == '/')
    {
        k_strncpy(resolved, pathname, resolved_size);
        resolved[resolved_size - 1] = '\0';
        vfs_normalize_path(resolved, resolved_size);
        return;
    }

    /* Relative: prepend CWD (or "/") */
    if (cwd[0] != '\0')
    {
        k_strncpy(resolved, cwd, resolved_size);
        resolved[resolved_size - 1] = '\0';
    }
    else
    {
        resolved[0] = '/';
        if (resolved_size > 1)
        {
            resolved[1] = '\0';
        }
    }

    size_t len = k_strlen(resolved);

    if (len == 0)
    {
        resolved[0] = '/';
        if (resolved_size > 1)
        {
            resolved[1] = '\0';
        }
        len = 1;
    }

    /* Add separator if needed */
    if (len < resolved_size - 1 && resolved[len - 1] != '/')
    {
        resolved[len++] = '/';
        resolved[len] = '\0';
    }

    /* Append relative pathname */
    if (len < resolved_size - 1)
    {
        k_strncpy(resolved + len, pathname, resolved_size - len);
        resolved[resolved_size - 1] = '\0';
    }

    /* Normalize final absolute path */
    vfs_normalize_path(resolved, resolved_size);
}

int vfs_open(struct task *task, const char *pathname, int flags, int mode)
{
    char resolved_path[MAX_FILENAME_LEN];
    vfs_resolve_path(task->cwd, pathname, resolved_path, sizeof(resolved_path));

    struct fs *fs = path_to_fs(resolved_path);
    if (fs == NULL)
    {
        kprintf("vfs_open: no fs found\n");
        return -1;
    }

    if (task == NULL)
    {
        kprintf("vfs_open: no task found\n");
        return -1;
    }

    struct file *file = vfs_alloc_file();
    if (file == NULL)
    {
        kprintf("vfs_open: can't allocate file\n");
        return -1;
    }

    int fd = files_alloc_fd(&task->files, file);
    if (fd < 0)
    {
        kprintf("vfs_open: can't allocate fd\n");
        vfs_free_file(file);
        return -1;
    }

    file->driver_data = NULL;
    file->fd = fd;
    file->pos = 0;
    file->flags = flags;
    file->mode = mode;
    file->file_ops.getdents = NULL;
    file->file_ops.close = NULL;
    file->file_ops.write = NULL;
    file->file_ops.read = NULL;

    k_strcpy(file->pathname, resolved_path);

    // Call filesystem-specific open if it exists
    if (fs->open != NULL)
    {
        int res = fs->open(file);
        if (res < 0)
        {
            files_free_fd(&task->files, fd);
            vfs_free_file(file);
            return -1;
        }
    }

     return fd;
}

int vfs_close(struct task *task, const int fd)
{
    if (fd < 0 || task == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&task->files, fd);
    if (file == NULL)
    {
        return -1;
    }

    if (file->file_ops.close != NULL)
    {
        file->file_ops.close(file);
    }

    files_free_fd(&task->files, fd);
    vfs_free_file(file);
    return 0;
}

ssize_t vfs_write(int fd, const char *buf, size_t count)
{
    if (buf == NULL || count == 0)
    {
        return 0;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->file_ops.write == NULL)
    {
        return -1;
    }

    return file->file_ops.write(file, buf, count);
}

ssize_t vfs_read(int fd, void *buf, size_t count)
{
    sched_schedule();

    if (buf == NULL || count == 0)
    {
        return 0;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->file_ops.read == NULL)
    {
        return -1;
    }

    return file->file_ops.read(file, buf, count);
}

int vfs_getdents(int fd, struct dirent *buf, unsigned int count)
{
    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->file_ops.getdents == NULL)
    {
        return -1;
    }

    return file->file_ops.getdents(file, buf, count);
}

int vfs_chdir(const char *path)
{
    if (path == NULL)
    {
        return -EINVAL;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -ESRCH;
    }

    char resolved[MAX_FILENAME_LEN];

    vfs_resolve_path(current->cwd, path, resolved, sizeof(resolved));

    int fd = vfs_open(current, resolved, 0, 0);
    if (fd < 0)
    {
        return fd;
    }

    struct dirent test_dirent;
    int result = vfs_getdents(fd, &test_dirent, sizeof(test_dirent));
    vfs_close(current, fd);

    if (result == -ENOTDIR)
    {
        return -ENOTDIR;
    }

    if (result < 0)
    {
        return result;
    }

    // Copy the resolved path into task->cwd safely
    k_strncpy(current->cwd, resolved, sizeof(current->cwd));
    current->cwd[sizeof(current->cwd) - 1] = '\0';

    return 0;
}

char *vfs_getcwd(char *buf, size_t size)
{
    if (buf == NULL || size == 0)
    {
        return NULL;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        buf[0] = '\0';
        return NULL;
    }

    const char *cwd = current->cwd;

    /* Fallback if cwd is empty */
    if (cwd[0] == '\0')
    {
        cwd = "/";
    }

    size_t i = 0;
    while (i + 1 < size && cwd[i] != '\0')
    {
        buf[i] = cwd[i];
        i++;
    }

    if (cwd[i] != '\0')
    {
        /* buffer too small */
        buf[0] = '\0';
        return NULL;
    }

    buf[i] = '\0';
    return buf;
}