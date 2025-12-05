#include <stdint.h>
#include "../include/kernel/libc.h"

/* handy macro so you don’t type row/attr all the time */
#define DBG(row, msg) vga_puts_at((row), (msg), 0x07)

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

/* store PID of last created task */
static pid_t last_pid = -1;

#define LINE_MAX       128
#define HISTORY_MAX    30

static char history[HISTORY_MAX][LINE_MAX];
static int  history_size = 0;   // how many entries currently stored (<= HISTORY_MAX)
static int  history_pos  = 0;   // cursor into history [0..history_size], history_size == "current line"

/* helper: add a line to history (drop oldest if full) */
static void history_add(const char *line)
{
    if (line[0] == '\0') {
        return; // don't store empty commands
    }

    if (history_size < HISTORY_MAX) {
        strcpy(history[history_size], line);
        history_size++;
    } else {
        // shift everything up and overwrite last
        for (int i = 1; i < HISTORY_MAX; i++) {
            strcpy(history[i - 1], history[i]);
        }
        strcpy(history[HISTORY_MAX - 1], line);
    }

    history_pos = history_size; // reset browsing position to "after last"
}

/* helper: replace current input line on screen with given text */
static void load_history_entry(char *line, size_t *len, int new_index)
{
    // erase current line content (after prompt) using backspace
    while (*len > 0) {
        write(FD_STDOUT, "\b \b", 3);
        (*len)--;
    }

    if (new_index == history_size) {
        // "current line" (empty)
        line[0] = '\0';
        *len = 0;
        return;
    }

    // copy from history[new_index] into line
    strcpy(line, history[new_index]);
    *len = strlen(line);

    // print new line contents
    write(FD_STDOUT, line, *len);
}

/* handle escape sequences for arrow keys: ESC [ A / ESC [ B */
static int handle_escape_sequence(char *line, size_t *len)
{
    char seq1, seq2;
    ssize_t n1 = read(FD_STDIN, &seq1, 1);
    if (n1 <= 0) return 0;

    ssize_t n2 = read(FD_STDIN, &seq2, 1);
    if (n2 <= 0) return 0;

    if (seq1 == '[') {
        if (seq2 == 'A') {
            // Up arrow
            if (history_size > 0 && history_pos > 0) {
                history_pos--;
                load_history_entry(line, len, history_pos);
            }
            return 1;
        } else if (seq2 == 'B') {
            // Down arrow
            if (history_size > 0 && history_pos < history_size) {
                history_pos++;
                load_history_entry(line, len, history_pos);
            }
            return 1;
        }
    }

    // Unknown escape sequence: ignore
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 1)
    {
        printf("process2: only accept 0 argument, but received %d, exiting\n", argc);
        return 1;
    }

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
                history_pos = history_size; // reset history cursor
                state = STATE_READ;
                break;

            case STATE_READ:
            {
                char c;
                ssize_t n = read(FD_STDIN, &c, 1);

                if (n <= 0)
                {
                    // nothing to read this tick; just move on
                    break;
                }

                // handle arrow keys: ESC [ A / ESC [ B
                if (c == 0x1b)  // ESC
                {
                    if (handle_escape_sequence(line, &len)) {
                        break;
                    }
                    // if not recognized, fall through and ignore
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

                // store in history
                history_add(line);

                cmd_argc = 0;
                char *p = line;

                while (*p && cmd_argc < 15) // leave space for NULL terminator
                {
                    while (*p == ' ')
                        p++;
                    if (!*p) break;

                    cmd_argv[cmd_argc++] = p;

                    while (*p && *p != ' ')
                        p++;
                    if (*p) *p++ = '\0';
                }

                if (cmd_argc == 0) {
                    state = STATE_PROMPT;
                    break;
                }

                /* -------- built-in: echo (supports echo $!) -------- */
                if (strcmp(cmd_argv[0], "echo") == 0) {
                    if (cmd_argc == 1) {
                        // plain "echo"
                        printf("\n");
                    } else {
                        for (int i = 1; i < cmd_argc; i++) {
                            if (strcmp(cmd_argv[i], "$!") == 0) {
                                if (last_pid < 0) {
                                    printf("no last pid");
                                } else {
                                    printf("%d", last_pid);
                                }
                            } else {
                                printf("%s", cmd_argv[i]);
                            }
                            if (i < cmd_argc - 1)
                                printf(" ");
                        }
                        printf("\n");
                    }

                    state = STATE_PROMPT;
                    break;
                }

                /* -------- default: start a task via sched_add_task -------- */
                if (cmd_argc > 0) {
                    // NULL-terminate argv list for the new task
                    cmd_argv[cmd_argc] = NULL;

                    const char *filename = cmd_argv[0];
                    char fullpath[LINE_MAX];

                    // If not prefixed with "/bin/", add it
                    if (strncmp(filename, "/bin/", 5) != 0) {
                        const char *prefix = "/bin/";
                        size_t plen = strlen(prefix);
                        size_t nlen = strlen(filename);

                        // Very simple bound check; truncate if needed
                        if (plen + nlen >= sizeof(fullpath)) {
                            nlen = sizeof(fullpath) - plen - 1;
                        }

                        // build "/bin/" + filename
                        // (we know plen == 5 here)
                        fullpath[0] = '/';
                        fullpath[1] = 'b';
                        fullpath[2] = 'i';
                        fullpath[3] = 'n';
                        fullpath[4] = '/';
                        for (size_t i = 0; i < nlen; i++) {
                            fullpath[plen + i] = filename[i];
                        }
                        fullpath[plen + nlen] = '\0';

                        filename = fullpath;
                    }

                    pid_t pid = sched_add_task(filename, cmd_argc, cmd_argv);
                    if (pid < 0) {
                        printf("Failed to create task\n");
                    } else {
                        last_pid = pid;
                    }
                }

                state = STATE_PROMPT;
                break;
            }
        }

        /* cooperative scheduling: always sched_yield once per iteration */
        sched_yield();
    }

    return 0;
}
