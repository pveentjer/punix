#ifndef DEV_H
#define DEV_H

#include <stddef.h>
#include "files.h"


struct dev_ops
{
    int (*open)(struct file *file);

    int (*close)(struct file *file);

    ssize_t (*read)(struct file *file, void *buf, size_t count);

    ssize_t (*write)(struct file *file, const void *buf, size_t count);
};

int dev_register(const char *name, struct dev_ops *ops, void *driver_data);

void dev_init(void);

#endif // DEV_H