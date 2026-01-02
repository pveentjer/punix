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

    char namebuf[MAX_FILENAME_LEN];
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

    for (int k = 0; k < MAX_PROCESS_CNT; k++)
    {
        struct task *task = &sched.task_table.slots[k].task;
        if (task->state != TASK_POOLED)
        {
            fill_dirent_from_task(&buf[idx], task);
            idx++;
        }
    }

    return (int) idx;
}

/* ------------------------------------------------------------
 * Extract PID from /proc/<pid>[/...]
 * - must have at least one digit
 * - next char must be '\0' or '/'
 * ------------------------------------------------------------ */
static pid_t proc_path_to_pid(const char *pathname)
{
    if (!pathname || pathname[0] != '/')
        return PID_NONE;

    if (k_strncmp(pathname, "/proc/", 6) != 0)
        return PID_NONE;

    const char *p = pathname + 6;

    /* must start with a digit */
    if (*p < '0' || *p > '9')
        return PID_NONE;

    pid_t pid = 0;
    while (*p >= '0' && *p <= '9')
    {
        pid = pid * 10 + (*p - '0');
        p++;
    }

    /* must end or be followed by '/' */
    if (*p != '\0' && *p != '/')
        return PID_NONE;

    return pid;
}

/* ------------------------------------------------------------
 * Helper: is pathname exactly /proc or /proc/
 * ------------------------------------------------------------ */
static bool proc_is_root(const char *pathname)
{
    return pathname &&
           (k_strcmp(pathname, "/proc") == 0 ||
            k_strcmp(pathname, "/proc/") == 0);
}

/* ------------------------------------------------------------
 * Helper: is pathname exactly /proc/<pid> or /proc/<pid>/
 * ------------------------------------------------------------ */
static bool proc_is_pid_dir(const char *pathname, pid_t *pid_out)
{
    pid_t pid = proc_path_to_pid(pathname);
    if (pid == PID_NONE)
    {
        return false;
    }

    const char *p = pathname + 6;
    while (*p >= '0' && *p <= '9')
    {
        p++;
    }

    if (*p == '\0' || (*p == '/' && p[1] == '\0'))
    {
        if (pid_out)
        {
            *pid_out = pid;
        }
        return true;
    }

    return false;
}

/* ------------------------------------------------------------
 * proc_open: validate that the requested /proc path exists
 * ------------------------------------------------------------ */
static int proc_open(struct file *file)
{
    const char *pathname = file->pathname;

    /* Root /proc directory */
    if (proc_is_root(pathname))
    {
        file->pos = 0;
        return 0;
    }

    /* /proc/stat */
    if (k_strcmp(pathname, "/proc/stat") == 0)
    {
        file->pos = 0;
        return 0;
    }

    /* Everything else must be /proc/<pid>[/...] */
    pid_t pid = proc_path_to_pid(pathname);
    if (pid == PID_NONE)
    {
        return -1;
    }

    struct task *task =
            task_table_find_task_by_pid(&sched.task_table, pid);
    if (!task)
    {
        return -1;
    }

    const char *p = pathname + 6;
    while (*p >= '0' && *p <= '9')
    {
        p++;
    }

    /* "/proc/<pid>" */
    if (*p == '\0')
    {
        file->pos = 0;
        return 0;
    }

    /* "/proc/<pid>/" */
    if (*p == '/' && p[1] == '\0')
    {
        file->pos = 0;
        return 0;
    }

    /* "/proc/<pid>/<file>" */
    if (*p == '/')
    {
        const char *leaf = p + 1;

        if (k_strcmp(leaf, "comm") == 0 ||
            k_strcmp(leaf, "cmdline") == 0 ||
            k_strcmp(leaf, "stat") == 0)
        {
            file->pos = 0;
            return 0;
        }
    }

    return -1;
}

/* ------------------------------------------------------------
 * Forward declarations
 * ------------------------------------------------------------ */
static ssize_t read_proc_stat(
        struct file *file,
        void *buf,
        size_t count);

static ssize_t read_proc_pid_comm(
        struct file *file,
        void *buf,
        size_t count,
        const struct task *task);

static ssize_t read_proc_pid_cmdline(
        struct file *file,
        void *buf,
        size_t count,
        const struct task *task);

static ssize_t read_proc_pid_stat(
        struct file *file,
        void *buf,
        size_t count,
        const struct task *task);

/* ------------------------------------------------------------
 * proc_read
 * ------------------------------------------------------------ */
static ssize_t proc_read(struct file *file, void *buf, size_t count)
{
    if (file->pos > 0 || !buf || count == 0)
    {
        return 0;
    }

    if (k_strcmp(file->pathname, "/proc/stat") == 0)
    {
        return read_proc_stat(file, buf, count);
    }

    pid_t pid = proc_path_to_pid(file->pathname);
    if (pid == PID_NONE)
    {
        return -1;
    }

    struct task *task =
            task_table_find_task_by_pid(&sched.task_table, pid);
    if (!task)
    {
        return -1;
    }

    const char *leaf = k_strrchr(file->pathname, '/');
    if (!leaf)
    {
        return -1;
    }
    leaf++;

    if (k_strcmp(leaf, "comm") == 0)
    {
        return read_proc_pid_comm(file, buf, count, task);
    }
    else if (k_strcmp(leaf, "cmdline") == 0)
    {
        return read_proc_pid_cmdline(file, buf, count, task);
    }
    else if (k_strcmp(leaf, "stat") == 0)
    {
        return read_proc_pid_stat(file, buf, count, task);
    }

    return -1;
}

static ssize_t read_proc_pid_cmdline(struct file *file, void *buf, size_t count, const struct task *task)
{
    size_t len = k_strlen(task->name);
    if (len > count)
    {
        len = count;
    }

    k_memcpy(buf, task->name, len);
    file->pos += len;
    return (ssize_t) len;
}

static ssize_t read_proc_pid_comm(struct file *file, void *buf, size_t count, const struct task *task)
{
    size_t len = k_strlen(task->name);
    if (len + 1 > count)
    {
        len = count - 1;
    }

    k_memcpy(buf, task->name, len);
    ((char *) buf)[len] = '\n';

    ssize_t size = (ssize_t) (len + 1);
    file->pos += size;
    return size;
}

/* /proc/<pid>/stat -> prints task->ctxt as decimal + '\n' */
static ssize_t read_proc_pid_stat(struct file *file, void *buf, size_t count, const struct task *task)
{
    /* Convert ctxt to string */
    char num[64];
    u64_to_str((uint64_t) task->ctxt, num, sizeof(num));

    size_t num_len = k_strlen(num);
    size_t len = num_len + 1; /* newline */

    if (len > count)
    {
        len = count;
    }

    if (len == 0)
    {
        return 0;
    }

    /* Copy digits */
    size_t digits_to_copy = (len > 0) ? (len - 1) : 0;
    if (digits_to_copy > num_len)
    {
        digits_to_copy = num_len;
    }

    k_memcpy(buf, num, digits_to_copy);

    /* Add newline if we have space for it */
    if (len > digits_to_copy)
    {
        ((char *) buf)[digits_to_copy] = '\n';
    }

    file->pos += len;
    return (ssize_t) len;
}

static ssize_t read_proc_stat(struct file *file, void *buf, size_t count)
{
    struct sched_stat st;
    sched_stat(&st);

    char num[64];
    u64_to_str(st.ctxt, num, sizeof(num));

    const char *prefix = "ctxt = ";
    size_t prefix_len = k_strlen(prefix);
    size_t num_len = k_strlen(num);

    size_t len = prefix_len + num_len + 1;
    if (len > count)
    {
        len = count;
    }

    k_memcpy(buf, prefix, prefix_len);
    k_memcpy((char *) buf + prefix_len, num, num_len);
    ((char *) buf)[prefix_len + num_len] = '\n';

    ssize_t size = (ssize_t) len;
    file->pos += size;
    return size;
}

/* ------------------------------------------------------------
 * proc_getdents:
 *  - list /proc root
 *  - list /proc/<pid> directory (comm, cmdline, stat)
 * ------------------------------------------------------------ */
static int proc_getdents(struct file *file, struct dirent *buf, unsigned int count)
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

    /* Root directory listing */
    if (proc_is_root(file->pathname))
    {
        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "stat");

        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            int written = sched_fill_proc_dirents(&buf[idx], remaining);
            if (written > 0)
            {
                idx += (unsigned int) written;
            }
        }

        int size = (int) (idx * sizeof(struct dirent));
        file->pos += size;
        return size;
    }

    /* /proc/<pid> directory listing */
    pid_t pid;
    if (proc_is_pid_dir(file->pathname, &pid))
    {
        struct task *task = task_table_find_task_by_pid(&sched.task_table, pid);
        if (!task)
        {
            return -1;
        }

        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "comm");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "cmdline");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "stat");

        int size = (int) (idx * sizeof(struct dirent));
        file->pos += size;
        return size;
    }

    return -1;
}

/* ------------------------------------------------------------
 * Filesystem descriptor
 * ------------------------------------------------------------ */
struct fs proc_fs = {
        .open     = proc_open,
        .close    = NULL,
        .read     = proc_read,
        .write    = NULL,
        .getdents = proc_getdents
};
