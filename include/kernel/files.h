#ifndef FILES_H
#define FILES_H

#include <stdint.h>
#include <stdbool.h>
#include "constants.h"
#include "dirent.h"
#include "tty.h"
#include "sys/types.h"

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

#define O_RDONLY   0x0
#define O_WRONLY   0x1
#define O_RDWR     0x2
#define O_CREAT    0x40
#define O_TRUNC    0x200

struct files_slot
{
    struct file *file;
};

struct files
{
    struct files_slot slots[RLIMIT_NOFILE];
};

struct file_ops{

    int (*open)(struct file *file);

    int (*close)(struct file *file);

    ssize_t (*read)(struct file *file, void *buf, size_t count);

    ssize_t (*write)(struct file *file, const void *buf, size_t count);

    int (*getdents)(struct file *file, struct dirent *buf, unsigned int count);
};

// todo: files are currently not yet shared when doing a fork (since there is no fork)
struct file
{
    uint32_t idx;
    char pathname[MAX_FILENAME_LEN];
    int flags;
    int mode;
    // ref-count because when a process is forked, there will be multiple
    // process that have a fd to the same file instance.
    uint8_t ref_count;
    uint64_t pos;
    int fd;
    void *driver_data;
    struct file_ops file_ops;
};

struct fs
{
    int (*open)(struct file *file);

};

void files_init(struct files *files);

int files_alloc_fd(struct files *files, struct file *file);

struct file *files_free_fd(struct files *files, int fd);

struct file *files_find_by_fd(const struct files *files, int fd);

#endif //FILES_H