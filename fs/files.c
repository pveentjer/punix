#include <stddef.h>
#include "kernel/files.h"
#include "kernel/kutils.h"

#define FD_MAX      INT_MAX

#define MAX_GENERATION   (FD_MAX / MAX_FILE_CNT)

#define FILES_FD_MASK (RLIMIT_NOFILE - 1)


void files_init(
        struct files *files)
{
    files->free_head = 0;
    files->free_tail = RLIMIT_NOFILE;

    for (int slot_idx = 0; slot_idx < RLIMIT_NOFILE; slot_idx++)
    {
        struct files_slot *slot = &files->slots[slot_idx];
        slot->generation = 0;
        slot->file = NULL;
        slot->fd = FD_NONE;
        files->free_ring[slot_idx] = slot_idx;
    }
}

struct file *files_find_by_fd(
        const struct files *files,
        const int fd)
{
    if (fd < 0)
    {
        return NULL;
    }

    uint32_t slot_idx = fd & FILES_FD_MASK;
    struct files_slot *slot = &files->slots[slot_idx];
    if (slot->fd != fd)
    {
        return NULL;
    }
    else
    {
        return slot->file;
    }
}

int files_alloc_fd(
        struct files *files,
        struct file *file)
{
    if (files->free_head == files->free_tail)
    {
        return FD_NONE;
    }

    if (file == NULL)
    {
        panic("files_alloc_fd: file can't be null");
    }

    const uint32_t free_ring_idx = (files->free_head) & FILES_FD_MASK;
    const uint32_t slot_idx = files->free_ring[free_ring_idx];

    struct files_slot *slot = &files->slots[slot_idx];

    slot->file = file;
    slot->fd = slot->generation * RLIMIT_NOFILE + slot_idx;
    // we need to take care of the wrap around of the slot->generation
    slot->generation = (slot->generation + 1) & MAX_GENERATION;
    files->free_head++;
    return slot->fd;
}

struct file *files_free_fd(
        struct files *files,
        int fd)
{
    if (files->free_tail - files->free_head == RLIMIT_NOFILE)
    {
        panic("files_free: too many frees.");
    }

    if (fd < 0)
    {
        return NULL;
    }

    const uint32_t slot_idx = fd & FILES_FD_MASK;
    const uint32_t free_ring_idx = files->free_tail & FILES_FD_MASK;

    struct files_slot *slot = &files->slots[slot_idx];
    if (slot->fd != fd)
    {
        // a fd is freed that doesn't exist anymore.
        return NULL;
    }

    struct file *file = slot->file;
    files->free_ring[free_ring_idx] = slot_idx;
    files->free_tail++;
    slot->file = NULL;
    slot->fd = FD_NONE;
    return file;
}

