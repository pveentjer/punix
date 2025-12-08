#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/fcntl.h"
#include "../include/kernel/dirent.h"
#include "../include/kernel/vga.h"   // for vga_puts_at

#define DBG(row, msg) vga_puts_at((row), (msg), 0x07)

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

#define LINE_MAX        128
#define HISTORY_MAX     30
#define MAX_BIN_ENTRIES 128
#define MAX_NAME_LEN    64

/* store PID of last created task */
static pid_t last_pid = -1;

/* command history */
static char history[HISTORY_MAX][LINE_MAX];
static int history_size = 0;
static int history_pos = 0;

/* cached /bin entries */
static char bin_entries[MAX_BIN_ENTRIES][MAX_NAME_LEN];
static int bin_count = 0;

/* ------------------------------------------------------------------
 * Load /bin entries using open/getdents/close
 * ------------------------------------------------------------------ */
static void load_bin_entries(void)
{
    int fd = open("/bin", O_RDONLY, 0);
    if (fd < 0)
    {
        printf("shell: failed to open /bin\n");
        return;
    }

    char buf[512];
    ssize_t nread;

    while ((nread = getdents(fd, (struct dirent *) buf, sizeof(buf))) > 0)
    {
        size_t offset = 0;
        while (offset < (size_t) nread)
        {
            struct dirent *d = (struct dirent *) (buf + offset);

            /* skip . and .. */
            if (!(d->d_name[0] == '.' &&
                  (d->d_name[1] == '\0' ||
                   (d->d_name[1] == '.' && d->d_name[2] == '\0'))))
            {
                if (bin_count < MAX_BIN_ENTRIES)
                {
                    strncpy(bin_entries[bin_count],
                            d->d_name,
                            MAX_NAME_LEN - 1);
                    bin_entries[bin_count][MAX_NAME_LEN - 1] = '\0';
                    bin_count++;
                }
            }

            if (d->d_reclen == 0)
            {
                /* kernel bug guard; avoid infinite loop */
                break;
            }
            offset += d->d_reclen;
        }
    }

    close(fd);

    printf("Loaded %d programs from /bin\n", bin_count);
}

/* ------------------------------------------------------------------
 * helpers: history
 * ------------------------------------------------------------------ */
static void history_add(const char *line)
{
    if (line[0] == '\0') return;

    if (history_size < HISTORY_MAX)
    {
        strcpy(history[history_size], line);
        history_size++;
    }
    else
    {
        for (int i = 1; i < HISTORY_MAX; i++)
        {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_MAX - 1], line);
    }
    history_pos = history_size;
}

static void load_history_entry(char *line, size_t *len, int new_index)
{
    while (*len > 0)
    {
        write(FD_STDOUT, "\b \b", 3);
        (*len)--;
    }

    if (new_index == history_size)
    {
        line[0] = '\0';
        *len = 0;
        return;
    }

    strcpy(line, history[new_index]);
    *len = strlen(line);
    write(FD_STDOUT, line, *len);
}

/* handle arrow key escape sequences */
static int handle_escape_sequence(char *line, size_t *len)
{
    char seq1, seq2;
    ssize_t n1 = read(FD_STDIN, &seq1, 1);
    if (n1 <= 0) return 0;
    ssize_t n2 = read(FD_STDIN, &seq2, 1);
    if (n2 <= 0) return 0;

    if (seq1 == '[')
    {
        if (seq2 == 'A')
        { // up
            if (history_size > 0 && history_pos > 0)
            {
                history_pos--;
                load_history_entry(line, len, history_pos);
            }
            return 1;
        }
        else if (seq2 == 'B')
        { // down
            if (history_size > 0 && history_pos < history_size)
            {
                history_pos++;
                load_history_entry(line, len, history_pos);
            }
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------
 * check if a program name exists in cached /bin list
 * ------------------------------------------------------------------ */
static int is_in_bin(const char *name)
{
    for (int i = 0; i < bin_count; i++)
    {
        if (strcmp(bin_entries[i], name) == 0)
            return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------
 * Build "/bin/<name>" into dst without snprintf
 * dst_size includes space for terminating NUL.
 * ------------------------------------------------------------------ */
static void build_bin_path(char *dst, size_t dst_size, const char *name)
{
    const char *prefix = "/bin/";
    size_t plen = strlen(prefix);
    size_t nlen = strlen(name);

    if (dst_size == 0)
    {
        return;
    }

    /* copy prefix */
    size_t to_copy = plen;
    if (to_copy >= dst_size)
    {
        to_copy = dst_size - 1;
    }
    memcpy(dst, prefix, to_copy);

    size_t pos = to_copy;

    /* copy name (possibly truncated) */
    if (pos < dst_size - 1)
    {
        size_t remaining = dst_size - 1 - pos;
        size_t name_copy = (nlen > remaining) ? remaining : nlen;
        memcpy(dst + pos, name, name_copy);
        pos += name_copy;
    }

    dst[pos] = '\0';
}

/* ------------------------------------------------------------------
 * main shell
 * ------------------------------------------------------------------ */
int main(int argc, char **argv)
{
    if (argc != 1)
    {
        printf("shell: no arguments expected\n");
        return 1;
    }

//    load_bin_entries();

    char line[LINE_MAX];
    size_t len = 0;
    char *cmd_argv[16];
    int cmd_argc = 0;

    enum cli_state state = STATE_PROMPT;

    for (;;)
    {
        switch (state)
        {
            case STATE_PROMPT:
                printf("> ");
                len = 0;
                line[0] = '\0';
                history_pos = history_size;
                state = STATE_READ;
                break;

            case STATE_READ:
            {
                char c;
                ssize_t n = read(FD_STDIN, &c, 1);
                if (n <= 0) break;

                if (c == 0x1b)
                { // ESC
                    if (handle_escape_sequence(line, &len)) break;
                    break;
                }

                if (c == '\r' || c == '\n')
                {
                    line[len] = '\0';
                    write(FD_STDOUT, "\n", 1);
                    state = STATE_PARSE;
                    break;
                }

                if (c == '\b' || c == 127)
                {
                    if (len > 0)
                    {
                        len--;
                        write(FD_STDOUT, "\b \b", 3);
                    }
                    break;
                }

                if (len < sizeof(line) - 1)
                {
                    line[len++] = c;
                    write(FD_STDOUT, &c, 1);
                }
                break;
            }

            case STATE_PARSE:
            {
                if (len == 0)
                {
                    state = STATE_PROMPT;
                    break;
                }

                history_add(line);
                cmd_argc = 0;
                char *p = line;

                while (*p && cmd_argc < 15)
                {
                    while (*p == ' ')
                    { p++; }
                    if (!*p) break;
                    cmd_argv[cmd_argc++] = p;
                    while (*p && *p != ' ')
                    { p++; }
                    if (*p) *p++ = '\0';
                }
                cmd_argv[cmd_argc] = NULL;
                if (cmd_argc == 0)
                {
                    state = STATE_PROMPT;
                    break;
                }

                /* built-in: echo */
                if (strcmp(cmd_argv[0], "echo") == 0)
                {
                    if (cmd_argc == 1)
                    {
                        printf("\n");
                    }
                    else
                    {
                        for (int i = 1; i < cmd_argc; i++)
                        {
                            if (strcmp(cmd_argv[i], "$!") == 0)
                            {
                                if (last_pid < 0) printf("no last pid");
                                else printf("%d", last_pid);
                            }
                            else
                            {
                                printf("%s", cmd_argv[i]);
                            }
                            if (i < cmd_argc - 1) printf(" ");
                        }
                        printf("\n");
                    }
                    state = STATE_PROMPT;
                    break;
                }

                /* external command */
                const char *filename = cmd_argv[0];
                char fullpath[LINE_MAX];

                if (is_in_bin(filename))
                {
                    build_bin_path(fullpath, sizeof(fullpath), filename);
                    filename = fullpath;
                }

                pid_t pid = sched_add_task(filename, cmd_argc, cmd_argv);
                if (pid < 0)
                    printf("Failed to create task\n");
                else
                    last_pid = pid;

                state = STATE_PROMPT;
                break;
            }
        }
    }

    return 0;
}
