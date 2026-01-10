#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include "constants.h"
#include "dirent.h"
#include "files.h"
#include "sched.h"
#include "stat.h"

/* Opaque VFS structure - internals hidden */
struct vfs;

/* Forward declarations */
struct fs;

extern struct vfs vfs;

// External filesystem declarations
extern struct fs root_fs;
extern struct fs proc_fs;
extern struct fs bin_fs;
extern struct fs dev_fs;
extern struct fs sys_fs;

/* ------------------------------------------------------------------
 * Mounting API
 * ------------------------------------------------------------------ */

/* Mount a filesystem at a path */
int vfs_mount(const char *path, struct fs *fs);

void vfs_get_root_mounts(struct dirent *buf, unsigned int max_entries, unsigned int *idx);


/* ------------------------------------------------------------------
 * File API
 * ------------------------------------------------------------------ */

ssize_t vfs_write(int fd, const char *buf, size_t count);

ssize_t vfs_read(int fd, void *buf, size_t count);

int vfs_getdents(int fd, struct dirent *buf, unsigned int count);

int vfs_chdir(const char *path);

char *vfs_getcwd(char *buf, size_t size);

void vfs_resolve_path(
        const char *cwd,
        const char *pathname,
        char *resolved,
        size_t resolved_size);

void vfs_init(struct vfs *vfs);

int vfs_open(struct task *task, const char *pathname, int flags, int mode);

int vfs_close(struct task *task, int fd);

int vfs_fstat(struct task *task, int fd, struct stat *stat);

#endif //VFS_H