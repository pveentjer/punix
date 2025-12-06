#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "vfs.h"
#include "constants.h"
#include "dirent.h"
#include "constants.h"


/* Fake fds for virtual filesystems */
#define FD_PROC    4
#define FD_ROOT    5
#define FD_BIN     6

struct file
{
    int fd;
    char path[MAX_FILENAME_LEN];
};

struct vfs_slot
{
    struct file file;
    uint32_t generation;
};

struct vfs_table
{
    uint32_t free_ring[MAX_FILE_CNT];
    /* index used for allocation (pop from ring) */
    uint32_t free_head;
    /* index used for free (push into ring) */
    uint32_t free_tail;

    struct vfs_slot slots[MAX_FILE_CNT];
};

/* ------------------------------------------------------------------
 * API
 * ------------------------------------------------------------------ */

/* Initialize the VFS file table. */
void vfs_table_init(struct vfs_table *table);

/* Allocate a file object from the table (returns NULL if full). */
struct file *vfs_file_alloc(struct vfs_table *table);

/* Free a file object back into the table. */
void vfs_file_free(struct vfs_table *table, struct file *file);

/* Lookup a file by its file descriptor (returns NULL if invalid/stale). */
struct file *vfs_file_find_by_fd(
        const struct vfs_table *table,
        int32_t fd);


int vfs_getdents(int fd, struct dirent *buf, unsigned int count);

int vfs_open(const char *pathname, int flags, int mode);

int vfs_close(int fd);

#endif //VFS_H
