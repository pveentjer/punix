// vfs.c

#include "kernel/vfs.h"
#include "kernel/kutils.h"
#include "kernel/console.h"

#define VFS_RING_MASK     (MAX_FILE_CNT - 1)
#define VFS_MAX_SEGMENTS  64

// External filesystem declarations
extern struct fs root_fs;
extern struct fs proc_fs;
extern struct fs bin_fs;
extern struct fs dev_fs;

// ==================== VFS =======================================

struct vfs vfs;

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

static struct fs *path_to_fs(
        const char *pathname)
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
    else if (len == 4 && k_strncmp(p, "proc", 4) == 0)
    {
        return &proc_fs;
    }
    else if (len == 3 && k_strncmp(p, "bin", 3) == 0)
    {
        return &bin_fs;
    }
    else if (len == 3 && k_strncmp(p, "dev", 3) == 0)
    {
        return &dev_fs;
    }
    else
    {
        return NULL;
    }
}

struct file *vfs_alloc_file(
        struct vfs *vfs)
{
    if (vfs->free_head == vfs->free_tail)
    {
        return NULL;
    }

    const uint32_t free_ring_idx = vfs->free_head & VFS_RING_MASK;
    const uint32_t file_idx = vfs->free_ring[free_ring_idx];

    struct file *file = &vfs->files[file_idx];

    vfs->free_head++;
    return file;
}

void vfs_free_file(
        struct vfs *vfs,
        struct file *file)
{
    if (vfs->free_tail - vfs->free_head == MAX_FILE_CNT)
    {
        panic("vfs_free_file: too many frees");
    }

    file->fd = -1;

    const uint32_t file_idx = file->idx;
    const uint32_t free_ring_idx = vfs->free_tail & VFS_RING_MASK;

    vfs->free_ring[free_ring_idx] = file_idx;
    vfs->free_tail++;
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
static void vfs_normalize_path(
        char *path,
        size_t buf_size)
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
 * Path resolution:
 *   - combine task->cwd and pathname
 *   - normalize the resulting absolute path
 * ------------------------------------------------------------ */
void vfs_resolve_path(
        const char *cwd,
        const char *pathname,
        char *resolved,
        size_t resolved_size)
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

int vfs_open(
        struct vfs *vfs,
        struct task *task,
        const char *pathname,
        int flags,
        int mode)
{
    char resolved_path[MAX_FILENAME_LEN];
    vfs_resolve_path(task->cwd, pathname, resolved_path, sizeof(resolved_path));

    struct fs *fs = path_to_fs(resolved_path);
    if (fs == NULL)
    {
        return -1;
    }

    if (task == NULL)
    {
        return -1;
    }

    struct file *file = vfs_alloc_file(vfs);
    if (file == NULL)
    {
        return -1;
    }

    int fd = files_alloc_fd(&task->files, file);
    if (fd < 0)
    {
        vfs_free_file(vfs, file);
        return -1;
    }

    file->tty = NULL;
    file->fd = fd;
    file->pos = 0;
    file->flags = flags;
    file->mode = mode;
    file->fs = fs;
    k_strcpy(file->pathname, resolved_path);

    // Call filesystem-specific open if it exists
    if (fs->open != NULL)
    {
        int res = fs->open(file);

        if (res < 0)
        {
            files_free_fd(&task->files, fd);
            vfs_free_file(vfs, file);
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

    files_free_fd(&task->files, fd);
    vfs_free_file(vfs, file);
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
    if (file == NULL || file->fs == NULL || file->fs->write == NULL)
    {
        return -1;
    }

    return file->fs->write(file, buf, count);
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
    if (file == NULL || file->fs == NULL || file->fs->read == NULL)
    {
        return -1;
    }

    return file->fs->read(file, buf, count);
}

int vfs_getdents(int fd, struct dirent *buf, unsigned int count)
{
    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    struct file *file = files_find_by_fd(&current->files, fd);
    if (file == NULL || file->fs == NULL || file->fs->getdents == NULL)
    {
        return -1;
    }

    return file->fs->getdents(file, buf, count);
}

int vfs_chdir(const char *path)
{
    if (path == NULL)
    {
        return -1;
    }

    struct task *current = sched_current();
    if (current == NULL)
    {
        return -1;
    }

    char resolved[MAX_FILENAME_LEN];

    // Use VFS to resolve relative or absolute paths against current->cwd
    vfs_resolve_path(current->cwd, path, resolved, sizeof(resolved));


    // Optional check: try opening it (helps confirm existence)
    int fd = vfs_open(&vfs, current, resolved, 0, 0);
    if (fd < 0)
    {
        return -1;
    }
    vfs_close(&vfs, current, fd);

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

    const char *src = current->cwd;

    /* Fallback if cwd is empty */
    if (src[0] == '\0')
    {
        src = "/";
    }

    size_t i = 0;
    while (i + 1 < size && src[i] != '\0')
    {
        buf[i] = src[i];
        i++;
    }

    if (src[i] != '\0')
    {
        /* buffer too small */
        buf[0] = '\0';
        return NULL;
    }

    buf[i] = '\0';
    return buf;
}