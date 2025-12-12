#include <stdint.h>
#include "../include/kernel/libc.h"
#include "../include/kernel/fcntl.h"
#include "../include/kernel/dirent.h"

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

#define LINE_MAX        128
#define HISTORY_MAX     30
#define MAX_NAME_LEN    64

/* shell PID */
static pid_t shell_pid = 0;

/* store PID of last background task */
static pid_t last_bg_pid = -1;

/* last exit status */
static int last_exit_status = 0;

/* command history */
static char history[HISTORY_MAX][LINE_MAX];
static int history_size = 0;
static int history_pos = 0;

/* ------------------------------------------------------------------
 * Search /bin for a command
 * ------------------------------------------------------------------ */
static int find_in_bin(const char *name, char *fullpath, size_t fullpath_size)
{
    int fd = open("/bin", O_RDONLY, 0);
    if (fd < 0)
        return 0;

    char buf[4096];
    ssize_t nread;
    int found = 0;

    while ((nread = getdents(fd, (struct dirent *) buf, sizeof(buf))) > 0)
    {
        size_t offset = 0;
        while (offset < (size_t) nread)
        {
            struct dirent *d = (struct dirent *) (buf + offset);

            if (strcmp(d->d_name, name) == 0)
            {
                const char *prefix = "/bin/";
                size_t plen = strlen(prefix);
                size_t nlen = strlen(name);

                size_t to_copy = plen;
                if (to_copy >= fullpath_size)
                    to_copy = fullpath_size - 1;
                memcpy(fullpath, prefix, to_copy);

                if (to_copy < fullpath_size - 1)
                {
                    size_t remaining = fullpath_size - 1 - to_copy;
                    size_t name_copy = (nlen > remaining) ? remaining : nlen;
                    memcpy(fullpath + to_copy, name, name_copy);
                    to_copy += name_copy;
                }
                fullpath[to_copy] = '\0';

                found = 1;
                break;
            }

            if (d->d_reclen == 0)
                break;
            offset += d->d_reclen;
        }
        if (found)
            break;
    }

    close(fd);
    return found;
}

/* ------------------------------------------------------------------
 * helpers: history
 * ------------------------------------------------------------------ */
static void history_add(const char *line)
{
    if (line[0] == '\0')
        return;

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
    if (n1 <= 0)
        return 0;
    ssize_t n2 = read(FD_STDIN, &seq2, 1);
    if (n2 <= 0)
        return 0;

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
 * Resolve command name to full path
 * ------------------------------------------------------------------ */
static void resolve_full_path(char *dst, size_t dst_size, const char *cmd)
{
    if (!cmd || dst_size == 0)
    {
        if (dst_size > 0)
            dst[0] = '\0';
        return;
    }

    /* if cmd starts with '/', already absolute */
    if (cmd[0] == '/')
    {
        size_t len = strlen(cmd);
        if (len >= dst_size)
            len = dst_size - 1;
        memcpy(dst, cmd, len);
        dst[len] = '\0';
        return;
    }

    /* search /bin */
    if (find_in_bin(cmd, dst, dst_size))
    {
        return;
    }

    /* not found, copy as-is */
    size_t len = strlen(cmd);
    if (len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, cmd, len);
    dst[len] = '\0';
}

/* ------------------------------------------------------------------
 * Convert integer to string
 * ------------------------------------------------------------------ */
static void int_to_str(int n, char *buf, int buf_size)
{
    int i = 0;

    if (n == 0)
    {
        buf[i++] = '0';
        buf[i] = '\0';
        return;
    }

    int is_negative = 0;
    if (n < 0)
    {
        is_negative = 1;
        n = -n;
    }

    char tmp[16];
    int j = 0;
    while (n > 0 && j < 15)
    {
        tmp[j++] = '0' + (n % 10);
        n /= 10;
    }

    if (is_negative && i < buf_size - 1)
        buf[i++] = '-';

    for (int k = j - 1; k >= 0 && i < buf_size - 1; k--)
    {
        buf[i++] = tmp[k];
    }
    buf[i] = '\0';
}

/* ------------------------------------------------------------------
 * Expand variables ($?, $$, $!) in a string
 * ------------------------------------------------------------------ */
static void expand_variables(char *str)
{
    static char buf[LINE_MAX];
    char *src = str;
    char *dst = buf;

    while (*src && (dst - buf) < LINE_MAX - 1)
    {
        if (src[0] == '$')
        {
            if (src[1] == '?')
            {
                /* $? - last exit status */
                char num[16];
                int_to_str(last_exit_status, num, sizeof(num));

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else if (src[1] == '$')
            {
                /* $$ - shell PID */
                char num[16];
                int_to_str(shell_pid, num, sizeof(num));

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else if (src[1] == '!')
            {
                /* $! - last background PID */
                char num[16];
                int_to_str(last_bg_pid, num, sizeof(num));

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else
            {
                *dst++ = *src++;
            }
        }
        else
        {
            *dst++ = *src++;
        }
    }
    *dst = '\0';

    strcpy(str, buf);
}

/* ------------------------------------------------------------------
 * Parse arguments with quote support
 * Returns number of arguments parsed
 * ------------------------------------------------------------------ */
static int parse_arguments(char *line, char **argv, int max_args)
{
    static char arg_buf[16][LINE_MAX];
    int argc = 0;
    char *src = line;

    while (*src && argc < max_args)
    {
        /* skip spaces */
        while (*src == ' ')
            src++;
        if (!*src)
            break;

        char *dst = arg_buf[argc];
        int in_quotes = 0;

        /* collect one argument */
        while (*src)
        {
            if (*src == '"')
            {
                in_quotes = !in_quotes;
                src++;
                continue;
            }

            if (!in_quotes && *src == ' ')
                break;

            *dst++ = *src++;
        }

        *dst = '\0';
        argv[argc] = arg_buf[argc];
        argc++;
    }

    return argc;
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

    shell_pid = getpid();

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
                if (n <= 0)
                    break;

                if (c == 0x1b)
                { // ESC
                    if (handle_escape_sequence(line, &len))
                        break;
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

                /* parse with quote support */
                cmd_argc = parse_arguments(line, cmd_argv, 15);
                cmd_argv[cmd_argc] = NULL;

                if (cmd_argc == 0)
                {
                    state = STATE_PROMPT;
                    break;
                }

                /* expand variables in all arguments */
                for (int i = 0; i < cmd_argc; i++)
                {
                    expand_variables(cmd_argv[i]);
                }

                /* background: trailing & */
                int background = 0;
                if (cmd_argc > 0 && strcmp(cmd_argv[cmd_argc - 1], "&") == 0)
                {
                    background = 1;
                    cmd_argv[cmd_argc - 1] = NULL;
                    cmd_argc--;

                    if (cmd_argc == 0)
                    {
                        state = STATE_PROMPT;
                        break;
                    }
                }

                /* resolve command to full path */
                static char fullpath[LINE_MAX];
                resolve_full_path(fullpath, sizeof(fullpath), cmd_argv[0]);

                pid_t pid = sched_add_task(fullpath, cmd_argc, cmd_argv, -1);
                if (pid < 0)
                {
                    printf("Failed to create task\n");
                    last_exit_status = 1;
                }
                else
                {
                    if (background)
                    {
                        last_bg_pid = pid;
                    }

                    if (!background)
                    {
                        int status = 0;
                        pid_t res = waitpid(pid, &status, 0);
                        if (res < 0)
                        {
                            printf("waitpid failed for pid %d\n", (int) pid);
                            last_exit_status = 1;
                        }
                        else
                        {
                            last_exit_status = (status >> 8) & 0xff;
                        }
                    }
                }

                state = STATE_PROMPT;
                break;
            }
        }
    }

    return 0;
}