#include "kernel/files.h"
#include "kernel/kutils.h"

void files_init(struct files *files)
{
    for (int i = 0; i < RLIMIT_NOFILE; i++)
    {
        files->slots[i].file = NULL;
    }
}

struct file *files_find_by_fd(const struct files *files, int fd)
{
    if (fd < 0 || fd >= RLIMIT_NOFILE)
    {
        return NULL;
    }

    return files->slots[fd].file;
}

int files_alloc_fd(struct files *files, struct file *file)
{
    if (file == NULL)
    {
        panic("files_alloc_fd: file can't be null");
    }

    // Find lowest available fd (POSIX requirement)
    for (int fd = 0; fd < RLIMIT_NOFILE; fd++)
    {
        if (files->slots[fd].file == NULL)
        {
            files->slots[fd].file = file;
            return fd;
        }
    }

    return -1;  // No free slots
}

struct file *files_free_fd(struct files *files, int fd)
{
    if (fd < 0 || fd >= RLIMIT_NOFILE)
    {
        return NULL;
    }

    struct file *file = files->slots[fd].file;
    if (file == NULL)
    {
        return NULL;  // Already free
    }

    files->slots[fd].file = NULL;
    return file;
}
