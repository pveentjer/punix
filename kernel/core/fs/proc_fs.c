// proc_fs.c

#include "kernel/files.h"
#include "kernel/fs_util.h"
#include "kernel/sched.h"
#include "kernel/kutils.h"
#include "kernel/constants.h"

/* ------------------------------------------------------------
 * Helper: fill one dirent entry from a task
 * ------------------------------------------------------------ */
static void fill_dirent_from_task(struct dirent *de, const struct task *task)
{
    de->d_ino = (uint32_t) task->pid;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = DT_DIR;

    char namebuf[16];
    k_itoa(task->pid, namebuf);

    size_t nlen = k_strlen(namebuf);
    if (nlen >= sizeof(de->d_name))
    {
        nlen = sizeof(de->d_name) - 1;
    }

    k_memcpy(de->d_name, namebuf, nlen);
    de->d_name[nlen] = '\0';
}

/* ------------------------------------------------------------
 * Fill struct dirent entries for /proc
 * ------------------------------------------------------------ */
int sched_fill_proc_dirents(struct dirent *buf, unsigned int max_entries)
{
    unsigned int idx = 0;

    if (!buf || max_entries == 0)
    {
        return 0;
    }

    // Add the current task first (if any)
    if (sched.current && idx < max_entries)
    {
        fill_dirent_from_task(&buf[idx], sched.current);
        idx++;
    }

    // Add all other tasks in the run queue
    struct task *t = sched.run_queue.first;
    while (t && idx < max_entries)
    {
        if (t != sched.current)
        {
            fill_dirent_from_task(&buf[idx], t);
            idx++;
        }
        t = t->next;
    }

    return (int) idx;
}

/* Helper: extract PID from /proc/PID/... path */
static pid_t proc_path_to_pid(const char *pathname)
{
    if (!pathname || pathname[0] != '/')
    {
        return PID_NONE;
    }

    if (k_strncmp(pathname, "/proc/", 6) != 0)
    {
        return PID_NONE;
    }

    const char *p = pathname + 6;

    pid_t pid = 0;
    while (*p >= '0' && *p <= '9')
    {
        pid = pid * 10 + (*p - '0');
        p++;
    }

    return pid;
}

static ssize_t proc_read(struct file *file, void *buf, size_t count)
{
    if (!buf || count == 0)
    {
        return 0;
    }

    pid_t pid = proc_path_to_pid(file->pathname);
    if (pid == PID_NONE)
    {
        return -1;
    }

    struct task *task = task_table_find_task_by_pid(&sched.task_table, pid);
    if (task == NULL)
    {
        return -1;
    }

    const char *filename = k_strrchr(file->pathname, '/');
    if (!filename)
    {
        return -1;
    }
    filename++;

    if (k_strcmp(filename, "comm") == 0)
    {
        size_t len = k_strlen(task->name);
        if (len + 1 > count)
        {
            len = count - 1;
        }

        k_memcpy(buf, task->name, len);
        ((char *)buf)[len] = '\n';

        return (ssize_t)(len + 1);
    }
    else if (k_strcmp(filename, "cmdline") == 0)
    {
        size_t len = k_strlen(task->name);
        if (len > count)
        {
            len = count;
        }

        k_memcpy(buf, task->name, len);

        return (ssize_t)len;
    }

    return -1;
}

static int proc_getdents(struct file *file, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent))
    {
        return 0;
    }

    if (file->done)
    {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
    fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

    if (idx < max_entries)
    {
        unsigned int remaining = max_entries - idx;
        int written = sched_fill_proc_dirents(&buf[idx], remaining);
        if (written > 0)
        {
            idx += (unsigned int) written;
        }
    }

    file->done = true;
    return (int) (idx * sizeof(struct dirent));
}

struct fs proc_fs = {
        .open = NULL,
        .close = NULL,
        .read = proc_read,
        .write = NULL,
        .getdents = proc_getdents
};