#include <stdint.h>
#include "../include/kernel/libc.h"

/* Default signal is SIGTERM (15) on Linux */
#define SIGTERM_DEFAULT 15

/* Parse a signal spec: number or name (TERM, SIGTERM, KILL, SIGKILL, etc.) */
static int parse_signal(const char *s)
{
    /* numeric? */
    int is_digit = 1;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') {
            is_digit = 0;
            break;
        }
    }

    if (is_digit) {
        int sig = atoi(s);
        return (sig > 0) ? sig : -1;
    }

    /* strip optional "SIG" prefix */
    if (s[0] == 'S' && s[1] == 'I' && s[2] == 'G') {
        s += 3;
    }

    /* Very small mapping, extend as you add signals in your kernel */
    if (strcmp(s, "HUP") == 0)   return 1;
    if (strcmp(s, "INT") == 0)   return 2;
    if (strcmp(s, "QUIT") == 0)  return 3;
    if (strcmp(s, "KILL") == 0)  return 9;
    if (strcmp(s, "TERM") == 0)  return 15;
    if (strcmp(s, "STOP") == 0)  return 19;
    if (strcmp(s, "CONT") == 0)  return 18;

    return -1;  // unknown signal
}

static void print_usage(const char *prog)
{
    printf("usage: %s [-s signal] pid ...\n", prog);
    printf("       %s -signal pid ...\n", prog);
}

/*
 * Linux-like kill:
 *   kill PID
 *   kill -9 PID
 *   kill -TERM PID
 *   kill -s TERM PID
 */
int main(int argc, char **argv)
{
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    int sig = SIGTERM_DEFAULT;  // default SIGTERM
    int i = 1;

    /* Optional: kill -s SIGNAL pid ... */
    if (i < argc && strcmp(argv[i], "-s") == 0) {
        if (i + 1 >= argc) {
            printf("kill: missing signal after -s\n");
            print_usage(argv[0]);
            return 1;
        }
        sig = parse_signal(argv[i + 1]);
        if (sig <= 0) {
            printf("kill: invalid signal '%s'\n", argv[i + 1]);
            return 1;
        }
        i += 2;
    }
        /* Optional: kill -SIGNAL pid ... */
    else if (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        sig = parse_signal(argv[i] + 1); // skip leading '-'
        if (sig <= 0) {
            printf("kill: invalid signal '%s'\n", argv[i] + 1);
            return 1;
        }
        i++;
    }

    if (i >= argc) {
        printf("kill: missing pid\n");
        print_usage(argv[0]);
        return 1;
    }

    int exit_status = 0;

    /* Remaining args are PIDs */
    for (; i < argc; i++) {
        int pid = atoi(argv[i]);
        if (pid <= 0) {
            printf("kill: invalid pid '%s'\n", argv[i]);
            exit_status = 1;
            continue;
        }

        int rc = kill(pid, sig);
        if (rc < 0) {
            printf("kill: failed to send signal %d to pid %d\n", sig, pid);
            exit_status = 1;
        }
    }

    return exit_status;
}
