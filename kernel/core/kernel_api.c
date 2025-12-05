#include <stdint.h>
#include "../../include/kernel/kernel_api.h"
#include "../../include/kernel/vga.h"
#include "../../include/kernel/kutils.h"
#include "../../include/kernel/keyboard.h"
#include "../../include/kernel/sched.h"
#include "../../include/kernel/libc.h"
#include "../../include/kernel/dirent.h"
#include "../../include/kernel/elf.h"

#define FD_STDIN   0
#define FD_STDOUT  1
#define FD_STDERR  2

/* Fake fds for virtual filesystems */
#define FD_PROC    4
#define FD_ROOT    5
#define FD_BIN     6

#define ENOSYS 38

static ssize_t sys_write(int fd, const char *buf, size_t count)
{
    sched_yield();

    if (buf == 0 || count == 0)
        return 0;

    switch (fd)
    {
        case FD_STDOUT:
        case FD_STDERR:
            for (size_t i = 0; i < count; i++)
            {
                screen_put_char(buf[i]);
            }
            return (ssize_t) count;

        case FD_STDIN:
            return -1;

        default:
            return -1;
    }
}

static ssize_t sys_read(int fd, void *buf, size_t count)
{
    sched_yield();

    if (fd != FD_STDIN)
        return -1;

    if (!buf || count == 0)
        return 0;

    char *cbuf = (char *) buf;
    size_t read_cnt = 0;

    while (read_cnt < count && keyboard_has_char())
    {
        cbuf[read_cnt++] = keyboard_get_char();
    }

    return (ssize_t) read_cnt;
}

static pid_t sys_getpid(void)
{
    sched_yield();
    return sched_getpid();
}

static void sys_yield(void)
{
    sched_yield();
}

static void sys_exit(int status)
{
    sched_exit(status);
}

static pid_t sys_add_task(const char *filename, int argc, char **argv)
{
    sched_yield();
    return sched_add_task(filename, argc, argv);
}

static pid_t sys_fork(void)
{
    return -ENOSYS;
}

static int sys_execve(const char *pathname, char *const argv[], char *const envp[])
{
    return -ENOSYS;
}

static int sys_kill(pid_t pid, int sig)
{
    return sched_kill(pid, sig);
}

static int sys_nice(int inc)
{
    return 0;   // do nothing, succeed
}

static int sys_get_tasks(char *buf, int buf_size)
{
    return sched_get_tasks(buf, buf_size);
}


/* ------------------------------------------------------------
 * sys_open / sys_close for fake files
 * ------------------------------------------------------------ */

static int sys_open(const char *pathname, int flags, int mode)
{
    if (!pathname) return -ENOSYS;

    if (k_strcmp(pathname, "/") == 0 || k_strcmp(pathname, "/.") == 0)
    {
        return FD_ROOT;
    }

    if (k_strcmp(pathname, "/proc") == 0 || k_strcmp(pathname, "/proc/") == 0)
    {
        return FD_PROC;
    }

    if (k_strcmp(pathname, "/bin") == 0 || k_strcmp(pathname, "/bin/") == 0)
    {
        return FD_BIN;
    }

    /* optionally support opening /proc/<pid> or /bin/<name> later */
    return -ENOSYS;
}

static int sys_close(int fd)
{
    if (fd == FD_PROC || fd == FD_ROOT || fd == FD_BIN)
    {
        return 0;
    }
    return -ENOSYS;
}

/* ------------------------------------------------------------
 * Helper: add an entry into a struct dirent array inside user buffer
 *
 * buf_entries: pointer to start of struct dirent array
 * max_entries: how many struct dirent fit in that buffer
 * idx: pointer to current index (will be incremented on success)
 * ino: d_ino
 * d_type: d_type
 * name: NUL-terminated name to copy into d_name (truncated if necessary)
 *
 * returns 1 on success, 0 if no space
 * ------------------------------------------------------------ */
static int add_entry(struct dirent *buf_entries,
                     unsigned int max_entries,
                     unsigned int *idx,
                     uint32_t ino,
                     uint8_t d_type,
                     const char *name)
{
    if (*idx >= max_entries) return 0;

    struct dirent *de = &buf_entries[*idx];
    de->d_ino = ino;
    de->d_reclen = (uint16_t) sizeof(struct dirent);
    de->d_type = d_type;

    /* safe copy and ensure NUL termination */
    if (name)
    {
        size_t nlen = k_strlen(name);
        if (nlen >= sizeof(de->d_name)) nlen = sizeof(de->d_name) - 1;
        k_memcpy(de->d_name, name, nlen);
        de->d_name[nlen] = '\0';
    }
    else
    {
        de->d_name[0] = '\0';
    }

    (*idx)++;
    return 1;
}

/* ------------------------------------------------------------
 * getdents implementation for FD_PROC, FD_ROOT, FD_BIN
 * - expects buf to be large enough for an integral number of struct dirent
 * - returns number of bytes written (idx * sizeof(struct dirent))
 * - returns -ENOSYS for unsupported fd
 * ------------------------------------------------------------ */
int sys_getdents(int fd, struct dirent *buf, unsigned int count)
{
    if (!buf || count < sizeof(struct dirent)) {
        return 0;
    }

    unsigned int max_entries = count / sizeof(struct dirent);
    unsigned int idx = 0;

    if (fd == FD_ROOT)
    {
        /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* /proc and /bin entries */
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "proc");
        add_entry(buf, max_entries, &idx, 0, DT_DIR, "bin");

        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_PROC)
    {

       /* . and .. */
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        /* let scheduler fill the rest with PID dirs */
        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            int written = sched_fill_proc_dirents(&buf[idx], remaining);
            if (written > 0)
            {
                idx += (unsigned int) written;
            }
        }

        return (int) (idx * sizeof(struct dirent));
    }
    else if (fd == FD_BIN)
    {
        add_entry(buf, max_entries, &idx, 1, DT_DIR, ".");
        add_entry(buf, max_entries, &idx, 1, DT_DIR, "..");

        if (idx < max_entries)
        {
            unsigned int remaining = max_entries - idx;
            int written = elf_fill_bin_dirents(&buf[idx], remaining);
            if (written > 0)
                idx += (unsigned int) written;
        }

        return (int) (idx * sizeof(struct dirent));
    }

    return -ENOSYS;
}


/* ------------------------------------------------------------
 * Exported API instance in its own section
 * (make sure other function pointers are set elsewhere or add them here)
 * ------------------------------------------------------------ */
__attribute__((section(".kernel_api"), used))
const struct kernel_api kernel_api_instance = {
        .sys_write     = sys_write,
        .sys_read      = sys_read,
        .sys_getpid    = sys_getpid,
        .sys_yield     = sys_yield,
        .sys_exit      = sys_exit,
        .sys_execve    = sys_execve,
        .sys_fork      = sys_fork,
        .sys_kill      = sys_kill,
        .sys_add_task  = sys_add_task,
        .sys_nice      = sys_nice,
        .sys_get_tasks = sys_get_tasks,
        .sys_open      = sys_open,
        .sys_close     = sys_close,
        .sys_getdents      = sys_getdents
};
