#include <stdint.h>
#include "string.h"
#include "fcntl.h"
#include "dirent.h"
#include "stdio.h"
#include "unistd.h"
#include "stdlib.h"

extern char **environ;

enum cli_state
{
    STATE_PROMPT,
    STATE_READ,
    STATE_PARSE
};

#define LINE_MAX        128
#define HISTORY_MAX     30
#define MAX_NAME_LEN    64
#define MAX_VARS        64
#define MAX_VAR_NAME    64
#define MAX_VAR_VALUE   256

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

/* shell variables */
struct shell_var {
    char name[MAX_VAR_NAME];
    char value[MAX_VAR_VALUE];
    int exported;  /* 1 if should be in environment */
};

static struct shell_var variables[MAX_VARS];
static int var_count = 0;

/* ------------------------------------------------------------------
 * Variable management
 * ------------------------------------------------------------------ */
static int find_variable(const char *name)
{
    for (int i = 0; i < var_count; i++)
    {
        if (strcmp(variables[i].name, name) == 0)
            return i;
    }
    return -1;
}

static void set_variable(const char *name, const char *value, int exported)
{
    int idx = find_variable(name);

    if (idx >= 0)
    {
        /* update existing */
        strncpy(variables[idx].value, value, MAX_VAR_VALUE - 1);
        variables[idx].value[MAX_VAR_VALUE - 1] = '\0';
        if (exported)
            variables[idx].exported = 1;
    }
    else
    {
        /* add new */
        if (var_count >= MAX_VARS)
        {
            printf("Too many variables\n");
            return;
        }

        strncpy(variables[var_count].name, name, MAX_VAR_NAME - 1);
        variables[var_count].name[MAX_VAR_NAME - 1] = '\0';
        strncpy(variables[var_count].value, value, MAX_VAR_VALUE - 1);
        variables[var_count].value[MAX_VAR_VALUE - 1] = '\0';
        variables[var_count].exported = exported;
        var_count++;
    }
}

static const char *get_variable(const char *name)
{
    int idx = find_variable(name);
    if (idx >= 0)
        return variables[idx].value;
    return NULL;
}

static void unset_variable(const char *name)
{
    int idx = find_variable(name);
    if (idx >= 0)
    {
        /* shift remaining variables down */
        for (int i = idx; i < var_count - 1; i++)
        {
            variables[i] = variables[i + 1];
        }
        var_count--;
    }
}

static void export_variable(const char *name)
{
    int idx = find_variable(name);
    if (idx >= 0)
    {
        variables[idx].exported = 1;
    }
}

/* ------------------------------------------------------------------
 * Build environment array for child processes
 * ------------------------------------------------------------------ */
static char *envp_storage[MAX_VARS + 1];
static char envp_strings[MAX_VARS][MAX_VAR_NAME + MAX_VAR_VALUE + 2];

static char **build_environment(void)
{
    int env_idx = 0;

    for (int i = 0; i < var_count && env_idx < MAX_VARS; i++)
    {
        if (variables[i].exported)
        {
            /* format: NAME=VALUE */
            int offset = 0;
            const char *name = variables[i].name;
            const char *value = variables[i].value;

            while (*name && offset < MAX_VAR_NAME + MAX_VAR_VALUE)
                envp_strings[env_idx][offset++] = *name++;

            if (offset < MAX_VAR_NAME + MAX_VAR_VALUE)
                envp_strings[env_idx][offset++] = '=';

            while (*value && offset < MAX_VAR_NAME + MAX_VAR_VALUE)
                envp_strings[env_idx][offset++] = *value++;

            envp_strings[env_idx][offset] = '\0';
            envp_storage[env_idx] = envp_strings[env_idx];
            env_idx++;
        }
    }

    envp_storage[env_idx] = NULL;
    return envp_storage;
}

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
 * Expand variables ($VAR, $?, $$, $!) in a string
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
                snprintf(num, sizeof(num), "%d", last_exit_status);

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else if (src[1] == '$')
            {
                /* $$ - shell PID */
                char num[16];
                snprintf(num, sizeof(num), "%d", (int)shell_pid);

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else if (src[1] == '!')
            {
                /* $! - last background PID */
                char num[16];
                snprintf(num, sizeof(num), "%d", (int)last_bg_pid);

                for (char *p = num; *p && (dst - buf) < LINE_MAX - 1; p++)
                    *dst++ = *p;

                src += 2;
            }
            else if ((src[1] >= 'A' && src[1] <= 'Z') ||
                     (src[1] >= 'a' && src[1] <= 'z') ||
                     src[1] == '_')
            {
                /* $VAR - variable expansion */
                char varname[MAX_VAR_NAME];
                int vlen = 0;
                src++; /* skip $ */

                while ((*src >= 'A' && *src <= 'Z') ||
                       (*src >= 'a' && *src <= 'z') ||
                       (*src >= '0' && *src <= '9') ||
                       *src == '_')
                {
                    if (vlen < MAX_VAR_NAME - 1)
                        varname[vlen++] = *src;
                    src++;
                }
                varname[vlen] = '\0';

                const char *value = get_variable(varname);
                if (value)
                {
                    for (const char *p = value; *p && (dst - buf) < LINE_MAX - 1; p++)
                        *dst++ = *p;
                }
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
 * Check if line is a variable assignment (VAR=value)
 * Returns 1 if assignment, 0 otherwise
 * ------------------------------------------------------------------ */
static int parse_assignment(const char *arg, char *name, char *value)
{
    const char *eq = strchr(arg, '=');
    if (!eq || eq == arg)
        return 0;

    /* check that everything before = is a valid variable name */
    for (const char *p = arg; p < eq; p++)
    {
        if (!((*p >= 'A' && *p <= 'Z') ||
              (*p >= 'a' && *p <= 'z') ||
              (*p >= '0' && *p <= '9') ||
              *p == '_'))
        {
            return 0;
        }

        /* first char can't be a digit */
        if (p == arg && *p >= '0' && *p <= '9')
            return 0;
    }

    /* extract name and value */
    int nlen = eq - arg;
    if (nlen >= MAX_VAR_NAME)
        nlen = MAX_VAR_NAME - 1;
    memcpy(name, arg, nlen);
    name[nlen] = '\0';

    const char *val_start = eq + 1;
    int vlen = strlen(val_start);
    if (vlen >= MAX_VAR_VALUE)
        vlen = MAX_VAR_VALUE - 1;
    memcpy(value, val_start, vlen);
    value[vlen] = '\0';

    return 1;
}

/* ------------------------------------------------------------------
 * Load environment variables from parent process
 * ------------------------------------------------------------------ */
static void load_environment(char **envp)
{
    if (!envp)
        return;

    for (int i = 0; envp[i] != NULL && var_count < MAX_VARS; i++)
    {
        char *env_entry = envp[i];
        char *eq = strchr(env_entry, '=');

        if (!eq || eq == env_entry)
            continue;

        char name[MAX_VAR_NAME];
        char value[MAX_VAR_VALUE];

        /* extract name */
        int nlen = eq - env_entry;
        if (nlen >= MAX_VAR_NAME)
            nlen = MAX_VAR_NAME - 1;
        memcpy(name, env_entry, nlen);
        name[nlen] = '\0';

        /* extract value */
        const char *val_start = eq + 1;
        int vlen = strlen(val_start);
        if (vlen >= MAX_VAR_VALUE)
            vlen = MAX_VAR_VALUE - 1;
        memcpy(value, val_start, vlen);
        value[vlen] = '\0';

        /* add as exported variable */
        set_variable(name, value, 1);
    }
}

/* ------------------------------------------------------------------
 * Built-in command implementations
 * ------------------------------------------------------------------ */

/* exit */
static int builtin_exit(int argc, char **argv)
{
    int exit_code = 0;

    if (argc > 1)
    {
        /* parse exit code from argv[1] */
        exit_code = atoi(argv[1]);
    }
    else
    {
        /* use last exit status */
        exit_code = last_exit_status;
    }

    exit(exit_code);
    return 1; /* never reached */
}

/* cd */
static int builtin_cd(int argc, char **argv)
{
    const char *path;

    if (argc < 2)
    {
        /* cd with no args - go to HOME */
        path = get_variable("HOME");
        if (!path)
            path = "/";
    }
    else if (argc > 2)
    {
        printf("cd: too many arguments\n");
        last_exit_status = 1;
        return 1;
    }
    else
    {
        path = argv[1];
    }

    if (chdir(path) < 0)
    {
        printf("cd: cannot change directory to '%s'\n", path);
        last_exit_status = 1;
    }
    else
    {
        last_exit_status = 0;
    }
    return 1;
}

/* set */
static int builtin_set(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    /* show all variables */
    for (int i = 0; i < var_count; i++)
    {
        printf("%s=%s", variables[i].name, variables[i].value);
        if (variables[i].exported)
            printf(" [exported]");
        printf("\n");
    }
    last_exit_status = 0;
    return 1;
}

/* env / printenv */
static int builtin_env_like(int argc, char **argv)
{
    if (argc > 1)
    {
        /* printenv VAR - show specific variable */
        const char *value = get_variable(argv[1]);
        if (value)
        {
            int idx = find_variable(argv[1]);
            if (idx >= 0 && variables[idx].exported)
            {
                printf("%s\n", value);
                last_exit_status = 0;
            }
            else
            {
                last_exit_status = 1;
            }
        }
        else
        {
            last_exit_status = 1;
        }
    }
    else
    {
        /* show all exported */
        for (int i = 0; i < var_count; i++)
        {
            if (variables[i].exported)
            {
                printf("%s=%s\n", variables[i].name, variables[i].value);
            }
        }
        last_exit_status = 0;
    }
    return 1;
}

/* export */
static int builtin_export(int argc, char **argv)
{
    if (argc < 2)
    {
        /* export with no args shows exported variables */
        for (int i = 0; i < var_count; i++)
        {
            if (variables[i].exported)
            {
                printf("export %s=%s\n", variables[i].name, variables[i].value);
            }
        }
        last_exit_status = 0;
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        char name[MAX_VAR_NAME];
        char value[MAX_VAR_VALUE];

        if (parse_assignment(argv[i], name, value))
        {
            /* export VAR=value */
            set_variable(name, value, 1);
        }
        else
        {
            /* export VAR - mark existing variable as exported */
            export_variable(argv[i]);
        }
    }
    last_exit_status = 0;
    return 1;
}

/* unset */
static int builtin_unset(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("unset: missing variable name\n");
        last_exit_status = 1;
        return 1;
    }

    for (int i = 1; i < argc; i++)
    {
        unset_variable(argv[i]);
    }
    last_exit_status = 0;
    return 1;
}

/* repeat - execute command n times */
static int builtin_repeat(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("Usage: repeat <count> <command> [args...]\n");
        last_exit_status = 1;
        return 1;
    }

    int count = atoi(argv[1]);
    if (count <= 0)
    {
        printf("repeat: count must be positive\n");
        last_exit_status = 1;
        return 1;
    }

    /* shift argv to get the actual command */
    char **repeat_argv = &argv[2];
    int repeat_argc = argc - 2;

    for (int i = 0; i < count; i++)
    {
        printf("[%d/%d]\n", i + 1, count);

        static char fullpath[LINE_MAX];
        resolve_full_path(fullpath, sizeof(fullpath), repeat_argv[0]);

        char **child_envp = build_environment();

        pid_t pid = sched_add_task(fullpath, -1, repeat_argv, child_envp);
        if (pid < 0)
        {
            printf("Failed to create task\n");
            last_exit_status = 1;
            return 1;
        }

        /* wait for completion */
        int status = 0;
        pid_t res = waitpid(pid, &status, 0);
        if (res < 0)
        {
            printf("waitpid failed for pid %d\n", (int)pid);
            last_exit_status = 1;
            return 1;
        }

        last_exit_status = (status >> 8) & 0xff;

        if (last_exit_status != 0)
        {
            break;
        }
    }

    return 1;
}

/* help - show help for builtin commands */
static int builtin_help(int argc, char **argv)
{
    if (argc == 1)
    {
        /* show all builtins */
        printf("Shell builtin commands:\n");
        printf("  cd [dir]              Change directory\n");
        printf("  exit [n]              Exit shell with status n\n");
        printf("  export [VAR[=val]]    Mark variables for export to child processes\n");
        printf("  unset VAR             Remove variable\n");
        printf("  set                   Display all variables\n");
        printf("  env                   Display exported variables\n");
        printf("  printenv [VAR]        Display exported variable(s)\n");
        printf("  repeat N CMD [args]   Execute command N times\n");
        printf("  help [command]        Show help for builtin commands\n");
        printf("\nSpecial variables:\n");
        printf("  $?                    Exit status of last command\n");
        printf("  $$                    Shell process ID\n");
        printf("  $!                    Last background process ID\n");
        last_exit_status = 0;
        return 1;
    }

    /* show help for specific command */
    const char *cmd = argv[1];

    if (strcmp(cmd, "cd") == 0)
    {
        printf("cd: cd [dir]\n");
        printf("  Change the current directory to DIR.\n");
        printf("  If DIR is not specified, change to $HOME.\n");
    }
    else if (strcmp(cmd, "exit") == 0)
    {
        printf("exit: exit [n]\n");
        printf("  Exit the shell with status N.\n");
        printf("  If N is omitted, use the exit status of last command.\n");
    }
    else if (strcmp(cmd, "export") == 0)
    {
        printf("export: export [VAR[=value] ...]\n");
        printf("  Mark variables to be passed to child processes.\n");
        printf("  With no arguments, display all exported variables.\n");
        printf("  Examples:\n");
        printf("    export PATH=/bin:/usr/bin\n");
        printf("    export MYVAR\n");
    }
    else if (strcmp(cmd, "unset") == 0)
    {
        printf("unset: unset VAR [VAR ...]\n");
        printf("  Remove variables.\n");
    }
    else if (strcmp(cmd, "set") == 0)
    {
        printf("set: set\n");
        printf("  Display all shell variables and their values.\n");
    }
    else if (strcmp(cmd, "env") == 0 || strcmp(cmd, "printenv") == 0)
    {
        printf("env/printenv: env | printenv [VAR]\n");
        printf("  Display exported environment variables.\n");
        printf("  With VAR, display only that variable's value.\n");
    }
    else if (strcmp(cmd, "repeat") == 0)
    {
        printf("repeat: repeat COUNT COMMAND [args ...]\n");
        printf("  Execute COMMAND with args COUNT times.\n");
        printf("  Shows iteration counter [N/COUNT] before each execution.\n");
        printf("  Stops early if command exits with non-zero status.\n");
        printf("  Example:\n");
        printf("    repeat 5 echo hello\n");
    }
    else if (strcmp(cmd, "help") == 0)
    {
        printf("help: help [command]\n");
        printf("  Display help for builtin commands.\n");
        printf("  With no arguments, show all builtins.\n");
        printf("  With COMMAND, show detailed help for that builtin.\n");
    }
    else
    {
        printf("help: no help for '%s'\n", cmd);
        last_exit_status = 1;
        return 1;
    }

    last_exit_status = 0;
    return 1;
}

/* ------------------------------------------------------------------
 * Built-in dispatch
 * ------------------------------------------------------------------ */
static int handle_builtin(int argc, char **argv)
{
    if (argc <= 0 || argv == NULL || argv[0] == NULL)
        return 0;

    if (strcmp(argv[0], "exit") == 0)
        return builtin_exit(argc, argv);

    if (strcmp(argv[0], "cd") == 0)
        return builtin_cd(argc, argv);

    if (strcmp(argv[0], "set") == 0)
        return builtin_set(argc, argv);

    if (strcmp(argv[0], "env") == 0 || strcmp(argv[0], "printenv") == 0)
        return builtin_env_like(argc, argv);

    if (strcmp(argv[0], "export") == 0)
        return builtin_export(argc, argv);

    if (strcmp(argv[0], "unset") == 0)
        return builtin_unset(argc, argv);

    if (strcmp(argv[0], "repeat") == 0)
        return builtin_repeat(argc, argv);

    if (strcmp(argv[0], "help") == 0)
        return builtin_help(argc, argv);

    return 0; /* not a builtin */
}

/* ------------------------------------------------------------------
 * Handle a complete command line
 * ------------------------------------------------------------------ */
static void process_command(char *line)
{
    char *cmd_argv[16];
    int cmd_argc;

    /* parse with quote support */
    cmd_argc = parse_arguments(line, cmd_argv, 15);
    cmd_argv[cmd_argc] = NULL;

    if (cmd_argc == 0)
    {
        return;
    }

    /* check for variable assignments before expanding */
    int first_non_assignment = 0;
    for (int i = 0; i < cmd_argc; i++)
    {
        char name[MAX_VAR_NAME];
        char value[MAX_VAR_VALUE];

        if (parse_assignment(cmd_argv[i], name, value))
        {
            set_variable(name, value, 0);
            first_non_assignment = i + 1;
        }
        else
        {
            break;
        }
    }

    /* if entire line was assignments, we're done */
    if (first_non_assignment >= cmd_argc)
    {
        last_exit_status = 0;
        return;
    }

    /* shift argv to remove assignments */
    if (first_non_assignment > 0)
    {
        for (int i = 0; i < cmd_argc - first_non_assignment; i++)
        {
            cmd_argv[i] = cmd_argv[i + first_non_assignment];
        }
        cmd_argc -= first_non_assignment;
        cmd_argv[cmd_argc] = NULL;
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
            return;
        }
    }

    /* check for built-in commands */
    if (handle_builtin(cmd_argc, cmd_argv))
    {
        return;
    }

    /* resolve command to full path */
    static char fullpath[LINE_MAX];
    resolve_full_path(fullpath, sizeof(fullpath), cmd_argv[0]);

    char **child_envp = build_environment();

    pid_t pid = sched_add_task(fullpath, -1, cmd_argv, child_envp);
    if (pid < 0)
    {
        printf("Failed to create task\n");
        last_exit_status = 1;
        return;
    }

    if (background)
    {
        last_bg_pid = pid;
        /* do not wait */
        return;
    }

    /* foreground: wait for completion */
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

    /* load environment variables from parent */
    load_environment(environ);

    /* set default variables if not already set */
    if (find_variable("PATH") < 0)
        set_variable("PATH", "/bin", 1);
    if (find_variable("HOME") < 0)
        set_variable("HOME", "/", 1);
    if (find_variable("SHELL") < 0)
        set_variable("SHELL", "/bin/shell", 1);

    char line[LINE_MAX];
    size_t len = 0;

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

                /* handle complete command line */
                process_command(line);

                state = STATE_PROMPT;
                break;
            }
        }
    }

    return 0;
}