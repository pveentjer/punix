// dev_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/kutils.h"
#include "kernel/dev.h"

#define MAX_DEVICES 32


struct dev_node
{
    char name[32];
    struct dev_ops *ops;
    void *driver_data;
    int active;
};

static struct dev_node devices[MAX_DEVICES];
static int device_count = 0;

void dev_init(void)
{
    for (int i = 0; i < MAX_DEVICES; i++)
    {
        devices[i].active = 0;
        devices[i].ops = NULL;
        devices[i].driver_data = NULL;
        devices[i].name[0] = '\0';
    }
    device_count = 0;
}


int dev_register(const char *name, struct dev_ops *ops, void *driver_data)
{
    if (!name || !ops || device_count >= MAX_DEVICES)
    {
        return -1;
    }

    if (name[0] == '/')
    {
        kprintf("dev_register: invalid name (starts with /): %s\n", name);
        return -1;
    }

    // Find free slot
    for (int i = 0; i < MAX_DEVICES; i++)
    {
        if (!devices[i].active)
        {
            devices[i].active = 1;
            k_strncpy(devices[i].name, name, sizeof(devices[i].name));
            devices[i].name[sizeof(devices[i].name) - 1] = '\0';
            devices[i].ops = ops;
            devices[i].driver_data = driver_data;
            device_count++;
            return 0;
        }
    }

    return -1;
}

struct dev_node *dev_lookup(const char *name)
{
    if (!name)
    {
        return NULL;
    }

    for (int i = 0; i < MAX_DEVICES; i++)
    {

        if (devices[i].active && k_strcmp(devices[i].name, name) == 0)
        {
            return &devices[i];
        }
    }

    return NULL;
}

static int dev_open(struct file *file)
{
    if (!file || !file->pathname)
    {
        return -1;
    }

    // Must start with /dev
    if (k_strncmp(file->pathname, "/dev", 4) != 0)
    {
        return -1;
    }

    // Opening /dev or /dev/ itself (directory listing)
    if (file->pathname[4] == '\0' ||
        (file->pathname[4] == '/' && file->pathname[5] == '\0'))
    {
        return 0;  // Allow directory operations
    }

    // Must have /dev/ prefix for devices
    if (file->pathname[4] != '/')
    {
        return -1;
    }

    // Get device name after /dev/
    const char *dev_name = file->pathname + 5;

    // Empty device name is invalid
    if (dev_name[0] == '\0')
    {
        return -1;
    }

    struct dev_node *dev = dev_lookup(dev_name);
    if (!dev)
    {
        kprintf("dev_open dev not found %s\n", dev_name);
        return -1;
    }

    file->driver_data = dev->driver_data;

    if (dev->ops->open)
    {
        return dev->ops->open(file);
    }

    return 0;
}

static int dev_close(struct file *file)
{
    if (!file || !file->pathname)
    {
        return -1;
    }

    const char *name = k_strrchr(file->pathname, '/');
    if (!name)
    {
        name = file->pathname;
    }
    else
    {
        name++;
    }

    struct dev_node *dev = dev_lookup(name);
    if (!dev)
    {
        return -1;
    }

    if (dev->ops->close)
    {
        return dev->ops->close(file);
    }

    return 0;
}

static ssize_t dev_read(struct file *file, void *buf, size_t count)
{
    if (!file || !file->pathname || !buf || count == 0)
    {
        return 0;
    }

    const char *name = k_strrchr(file->pathname, '/');
    if (!name)
    {
        name = file->pathname;
    }
    else
    {
        name++;
    }

    struct dev_node *dev = dev_lookup(name);
    if (!dev || !dev->ops->read)
    {
        return -1;
    }

    return dev->ops->read(file, buf, count);
}

static ssize_t dev_write(struct file *file, const void *buf, size_t count)
{
    if (!file || !file->pathname || !buf || count == 0)
    {
        return 0;
    }

    const char *name = k_strrchr(file->pathname, '/');
    if (!name)
    {
        name = file->pathname;
    }
    else
    {
        name++;
    }

    struct dev_node *dev = dev_lookup(name);
    if (!dev || !dev->ops->write)
    {
        return -1;
    }

    return dev->ops->write(file, buf, count);
}

static int dev_getdents(struct file *file, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    if (file->pos > 0)
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 2, DT_DIR, "..");

    for (int i = 0; i < MAX_DEVICES; i++)
    {
        if (idx >= max_entries)
        {
            break;
        }

        if (devices[i].active)
        {
            fs_add_entry(buf, max_entries, &idx, 100 + i, DT_CHR, devices[i].name);
        }
    }

    int size = (int) (idx * sizeof(struct dirent));
    file->pos += size;
    return size;
}

struct fs dev_fs = {
        .open     = dev_open,
        .close    = dev_close,
        .read     = dev_read,
        .write    = dev_write,
        .getdents = dev_getdents
};