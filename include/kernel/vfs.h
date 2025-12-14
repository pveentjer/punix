#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "constants.h"
#include "dirent.h"
#include "files.h"
#include "sched.h"

struct vfs
{
    uint32_t free_ring[MAX_FILE_CNT];
    /* index used for allocation (pop from ring) */
    uint32_t free_head;
    /* index used for free (push into ring) */
    uint32_t free_tail;

    struct file files[MAX_FILE_CNT];
};


extern struct vfs vfs;

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

void vfs_resolve_path(
        const char *cwd,
        const char *pathname,
        char *resolved,
        size_t resolved_size);

void vfs_init(
        struct vfs *vfs);

int vfs_open(
        struct vfs *vfs,
        struct task *task,
        const char *pathname,
        int flags,
        int mode);

int vfs_close(
        struct vfs *vfs,
        struct task *task,
        int fd);


#endif //VFS_H
