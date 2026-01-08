// proc_fs.c

#include "errno.h"
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
 * ------------------------------------------------------------ */
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

    if (*p < '0' || *p > '9')
    {
        return PID_NONE;
    }

    pid_t pid = 0;
    while (*p >= '0' && *p <= '9')
    {
        pid = pid * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0' && *p != '/')
    {
        return PID_NONE;
    }

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
 * Helper: is pathname /proc/<pid>/fd or /proc/<pid>/fd/
 * ------------------------------------------------------------ */
static bool proc_is_fd_dir(const char *pathname, pid_t *pid_out)
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

    if (*p != '/')
    {
        return false;
    }
    p++;

    if (k_strcmp(p, "fd") == 0 || k_strcmp(p, "fd/") == 0)
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
 * Helper: extract fd number from /proc/<pid>/fd/<num>
 * ------------------------------------------------------------ */
static int proc_extract_fd_num(const char *pathname)
{
    const char *p = pathname;

    const char *fd_pos = k_strstr(p, "/fd/");
    if (!fd_pos)
    {
        return -1;
    }

    p = fd_pos + 4;

    if (*p < '0' || *p > '9')
    {
        return -1;
    }

    int fd = 0;
    while (*p >= '0' && *p <= '9')
    {
        fd = fd * 10 + (*p - '0');
        p++;
    }

    if (*p != '\0')
    {
        return -1;
    }

    return fd;
}

/* ------------------------------------------------------------
 * Helper: convert task state to character
 * ------------------------------------------------------------ */
static char task_state_to_char(enum sched_state state)
{
    switch (state)
    {
        case TASK_RUNNING:
            return 'R';
        case TASK_QUEUED:
            return 'R';
        case TASK_INTERRUPTIBLE:
            return 'S';
        case TASK_UNINTERRUPTIBLE:
            return 'D';
        case TASK_ZOMBIE:
            return 'Z';
        case TASK_POOLED:
            return 'X';
        default:
            return '?';
    }
}

/* ------------------------------------------------------------
 * Helper: convert task state to string
 * ------------------------------------------------------------ */
static const char *task_state_to_str(enum sched_state state)
{
    switch (state)
    {
        case TASK_RUNNING:
            return "running";
        case TASK_QUEUED:
            return "runnable";
        case TASK_INTERRUPTIBLE:
            return "sleeping";
        case TASK_UNINTERRUPTIBLE:
            return "disk sleep";
        case TASK_ZOMBIE:
            return "zombie";
        case TASK_POOLED:
            return "dead";
        default:
            return "unknown";
    }
}

/* ------------------------------------------------------------
 * Read functions - defined before proc_read uses them
 * ------------------------------------------------------------ */

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

/* /proc/<pid>/stat -> Linux-style format
 * Format: pid (comm) state ppid pgrp session ctxt syscalls brk brk_limit
 */
static ssize_t read_proc_pid_stat(struct file *file, void *buf, size_t count, const struct task *task)
{
    char output[512];
    char pid_str[32];
    char ppid_str[32];
    char ctxt_str[32];
    char syscall_str[32];
    char brk_str[32];
    char brk_limit_str[32];

    k_itoa(task->pid, pid_str);
    k_itoa(task->parent ? task->parent->pid : 0, ppid_str);
    u64_to_str(task->ctxt, ctxt_str, sizeof(ctxt_str));
    u64_to_str(task->sys_call_cnt, syscall_str, sizeof(syscall_str));
    k_itoa(task->brk, brk_str);
    k_itoa(task->brk_limit, brk_limit_str);

    char state = task_state_to_char(task->state);

    k_strcpy(output, pid_str);
    k_strcat(output, " (");
    k_strcat(output, task->name);
    k_strcat(output, ") ");

    char state_str[2] = {state, '\0'};
    k_strcat(output, state_str);
    k_strcat(output, " ");

    k_strcat(output, ppid_str);
    k_strcat(output, " 0 0 ");
    k_strcat(output, ctxt_str);
    k_strcat(output, " ");
    k_strcat(output, syscall_str);
    k_strcat(output, " ");
    k_strcat(output, brk_str);
    k_strcat(output, " ");
    k_strcat(output, brk_limit_str);
    k_strcat(output, "\n");

    size_t len = k_strlen(output);
    if (len > count)
    {
        len = count;
    }

    k_memcpy(buf, output, len);
    file->pos += len;
    return (ssize_t) len;
}

/* /proc/<pid>/status -> Human-readable status */
static ssize_t read_proc_pid_status(struct file *file, void *buf, size_t count, const struct task *task)
{
    char output[1024];
    char temp[256];

    output[0] = '\0';

    k_strcpy(output, "Name:\t");
    k_strcat(output, task->name);
    k_strcat(output, "\n");

    k_strcat(output, "State:\t");
    char state_char[2] = {task_state_to_char(task->state), '\0'};
    k_strcat(output, state_char);
    k_strcat(output, " (");
    k_strcat(output, task_state_to_str(task->state));
    k_strcat(output, ")\n");

    k_strcat(output, "Pid:\t");
    k_itoa(task->pid, temp);
    k_strcat(output, temp);
    k_strcat(output, "\n");

    k_strcat(output, "PPid:\t");
    k_itoa(task->parent ? task->parent->pid : 0, temp);
    k_strcat(output, temp);
    k_strcat(output, "\n");

    k_strcat(output, "Ctxt:\t");
    u64_to_str(task->ctxt, temp, sizeof(temp));
    k_strcat(output, temp);
    k_strcat(output, "\n");

    k_strcat(output, "Syscalls:\t");
    u64_to_str(task->sys_call_cnt, temp, sizeof(temp));
    k_strcat(output, temp);
    k_strcat(output, "\n");

    k_strcat(output, "Brk:\t0x");
    k_itoa_hex(task->brk, temp);
    k_strcat(output, temp);
    k_strcat(output, "\n");

    k_strcat(output, "BrkLimit:\t0x");
    k_itoa_hex(task->brk_limit, temp);
    k_strcat(output, temp);
    k_strcat(output, "\n");

    size_t len = k_strlen(output);
    if (len > count)
    {
        len = count;
    }

    k_memcpy(buf, output, len);
    file->pos += len;
    return (ssize_t) len;
}

static ssize_t read_proc_pid_cwd(struct file *file, void *buf, size_t count, const struct task *task)
{
    size_t len = k_strlen(task->cwd);
    if (len + 1 > count)
    {
        len = count - 1;
    }

    k_memcpy(buf, task->cwd, len);
    ((char *) buf)[len] = '\n';

    ssize_t size = (ssize_t) (len + 1);
    file->pos += size;
    return size;
}

static ssize_t read_proc_pid_exe(struct file *file, void *buf, size_t count, const struct task *task)
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

static ssize_t read_proc_pid_fd_link(struct file *file, void *buf, size_t count, const struct task *task, int fd)
{
    if (fd < 0 || fd >= RLIMIT_NOFILE)
    {
        return -1;
    }

    struct file *target = task->files.slots[fd].file;
    if (!target)
    {
        return -1;
    }

    const char *path = target->pathname ? target->pathname : "(unknown)";
    size_t len = k_strlen(path);

    if (len + 1 > count)
    {
        len = count - 1;
    }

    k_memcpy(buf, path, len);
    ((char *) buf)[len] = '\n';

    ssize_t size = (ssize_t) (len + 1);
    file->pos += size;
    return size;
}

static ssize_t read_proc_stat(struct file *file, void *buf, size_t count)
{
    struct sched_stat st;
    sched_stat(&st);

    char num[64];
    u64_to_str(st.ctxt, num, sizeof(num));

    const char *prefix = "ctxt ";
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

    int fd = proc_extract_fd_num(file->pathname);
    if (fd >= 0)
    {
        return read_proc_pid_fd_link(file, buf, count, task, fd);
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
    else if (k_strcmp(leaf, "status") == 0)
    {
        return read_proc_pid_status(file, buf, count, task);
    }
    else if (k_strcmp(leaf, "cwd") == 0)
    {
        return read_proc_pid_cwd(file, buf, count, task);
    }
    else if (k_strcmp(leaf, "exe") == 0)
    {
        return read_proc_pid_exe(file, buf, count, task);
    }

    return -1;
}

/* ------------------------------------------------------------
 * proc_getdents
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

    pid_t fd_pid;
    if (proc_is_fd_dir(file->pathname, &fd_pid))
    {
        struct task *task = task_table_find_task_by_pid(&sched.task_table, fd_pid);
        if (!task)
        {
            return -1;
        }

        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        for (int i = 0; i < RLIMIT_NOFILE && idx < max_entries; i++)
        {
            if (task->files.slots[i].file)
            {
                char fd_name[16];
                k_itoa(i, fd_name);
                fs_add_entry(buf, max_entries, &idx, i + 10, DT_LNK, fd_name);
            }
        }

        int size = (int) (idx * sizeof(struct dirent));
        file->pos += size;
        return size;
    }

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
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "cmdline");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "comm");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "cwd");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "exe");
        fs_add_entry(buf, max_entries, &idx, 1, DT_DIR, "fd");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "stat");
        fs_add_entry(buf, max_entries, &idx, 1, DT_REG, "status");

        int size = (int) (idx * sizeof(struct dirent));
        file->pos += size;
        return size;
    }

    /* Not a directory */
    return -ENOTDIR;
}

/* ------------------------------------------------------------
 * proc_open
 * ------------------------------------------------------------ */
static int proc_open(struct file *file)
{
    const char *pathname = file->pathname;

    file->file_ops.read = proc_read;
    file->file_ops.getdents = proc_getdents;

    if (proc_is_root(pathname))
    {
        return 0;
    }

    if (k_strcmp(pathname, "/proc/stat") == 0)
    {
        return 0;
    }

    pid_t pid = proc_path_to_pid(pathname);
    if (pid == PID_NONE)
    {
        return -1;
    }

    struct task *task = task_table_find_task_by_pid(&sched.task_table, pid);
    if (!task)
    {
        return -1;
    }

    const char *p = pathname + 6;
    while (*p >= '0' && *p <= '9')
    {
        p++;
    }

    if (*p == '\0')
    {
        return 0;
    }

    if (*p == '/' && p[1] == '\0')
    {
        return 0;
    }

    if (*p == '/')
    {
        const char *leaf = p + 1;

        if (k_strcmp(leaf, "comm") == 0 ||
            k_strcmp(leaf, "cmdline") == 0 ||
            k_strcmp(leaf, "stat") == 0 ||
            k_strcmp(leaf, "status") == 0 ||
            k_strcmp(leaf, "cwd") == 0 ||
            k_strcmp(leaf, "exe") == 0)
        {
            return 0;
        }

        if (k_strcmp(leaf, "fd") == 0 || k_strcmp(leaf, "fd/") == 0)
        {
            return 0;
        }

        if (k_strncmp(leaf, "fd/", 3) == 0)
        {
            int fd = proc_extract_fd_num(pathname);
            if (fd >= 0 && fd < RLIMIT_NOFILE)
            {
                if (task->files.slots[fd].file)
                {
                    file->pos = 0;
                    return 0;
                }
            }
            return -1;
        }
    }

    return -1;
}


/* ------------------------------------------------------------
 * Filesystem descriptor
 * ------------------------------------------------------------ */
struct fs proc_fs = {
        .open     = proc_open,
};