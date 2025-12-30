// time.c
#include "libc.h"
#include "string.h"
#include "stdio.h"
#include "time.h"
#include "stdio.h"

/*
 * Expect these to come from your headers:
 *  - struct timespec
 *  - CLOCK_MONOTONIC
 *  - clock_gettime(...)
 *  - sched_add_task(...)
 *  - waitpid(...)
 */

static uint64_t ts_to_ns(const struct timespec *ts)
{
    return (uint64_t) ts->tv_sec * 1000000000ULL + (uint64_t) ts->tv_nsec;
}

static void print_default(uint64_t real_ns)
{
    // Print seconds with 3 decimals (ms precision)
    uint64_t ms = real_ns / 1000000ULL;
    uint64_t sec = ms / 1000ULL;
    uint64_t rem_ms = ms % 1000ULL;

    printf("real\t%llu.%03llus\n",
           (unsigned long long) sec,
           (unsigned long long) rem_ms);

    // Not available without CPU accounting
    printf("user\t?\n");
    printf("sys \t?\n");
}

static void print_posix(uint64_t real_ns)
{
    uint64_t ms = real_ns / 1000000ULL;
    uint64_t sec = ms / 1000ULL;
    uint64_t rem_ms = ms % 1000ULL;

    printf("real %llu.%03llu\n",
           (unsigned long long) sec,
           (unsigned long long) rem_ms);

    printf("user ?\n");
    printf("sys  ?\n");
}

static void usage(void)
{
    printf("usage: time [-p] [--] command [args...]\n");
}

int main(int argc, char **argv, char **envp)
{
    int posix_p = 0;
    int i = 1;

    // Parse time's own options: -p, -- (end of options)
    for (; i < argc; i++)
    {
        const char *a = argv[i];

        if (strcmp(a, "--") == 0)
        {
            i++;
            break;
        }

        if (a[0] != '-')
        {
            break;
        }

        if (strcmp(a, "-p") == 0)
        {
            posix_p = 1;
            continue;
        }

        // Unknown option
        printf("time: unknown option: %s\n", a);
        usage();
        return 2;
    }

    if (i >= argc)
    {
        usage();
        return 2;
    }

    char **cmd_argv = &argv[i];

    // If your task loader expects a full path, your shell likely already resolved it.
    // We'll just pass argv[0] as given.
    const char *fullpath = cmd_argv[0];

    struct timespec start, end;

    // Start timestamp (monotonic)
    // If clock_gettime can fail in your OS, it likely returns < 0; we keep it simple.
    if (clock_gettime(CLOCK_MONOTONIC, &start) < 0)
    {
        printf("time: clock_gettime failed\n");
        return 1;
    }

    // Spawn + wait (foreground only; time(1) measures completion)
    pid_t pid = sched_add_task(fullpath, -1, cmd_argv, envp);
    if (pid < 0)
    {
        printf("time: Failed to create task\n");
        return 1;
    }

    int status = 0;
    pid_t res = waitpid(pid, &status, 0);
    if (res < 0)
    {
        printf("time: waitpid failed for pid %d\n", (int) pid);
        return 1;
    }

    // End timestamp
    if (clock_gettime(CLOCK_MONOTONIC, &end) < 0)
    {
        printf("time: clock_gettime failed\n");
        // Still return child exit status if we can
        return (status >> 8) & 0xff;
    }

    uint64_t start_ns = ts_to_ns(&start);
    uint64_t end_ns = ts_to_ns(&end);
    uint64_t real_ns = end_ns - start_ns;

    char buf[32];

    if (posix_p)
        print_posix(real_ns);
    else
        print_default(real_ns);

    // Return child's exit code (your convention)
    return (status >> 8) & 0xff;
}
