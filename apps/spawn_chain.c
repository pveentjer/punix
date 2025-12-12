#include <stdint.h>
#include "../include/kernel/libc.h"

#define MAX_DEPTH 100000  // safety guard

static void print_help(const char *prog)
{
    printf("Usage: %s --count <number>\n", prog);
    printf("\n");
    printf("Spawns a chain of child processes, each with count-1.\n");
    printf("Each process prints its PID and spawns the next until count reaches 0.\n");
    printf("\n");
    printf("Options:\n");
    printf("  --count <number>   Depth of the spawn chain (0-%d)\n", MAX_DEPTH);
    printf("  --help             Show this help message\n");
}

/* Convert a 32-bit unsigned integer to a decimal string */
static void u32_to_str(uint32_t number, char *out_str)
{
    char reversed_digits[11];
    int digit_count = 0;

    if (number == 0)
    {
        out_str[0] = '0';
        out_str[1] = '\0';
        return;
    }

    while (number > 0 && digit_count < 10)
    {
        reversed_digits[digit_count++] = (char) ('0' + (number % 10));
        number /= 10;
    }

    for (int i = 0; i < digit_count; i++)
    {
        out_str[i] = reversed_digits[digit_count - 1 - i];
    }
    out_str[digit_count] = '\0';
}

/* Parse named arguments like --count 10 */
static int32_t parse_named_args(int argc, char **argv, uint32_t *out_count)
{
    *out_count = 0;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--help") == 0)
        {
            print_help(argv[0]);
            return 1;  // signal that help was shown
        }
        else if (strcmp(argv[i], "--count") == 0)
        {
            if (i + 1 >= argc)
            {
                printf("error: missing value after --count\n");
                return -1;
            }
            int32_t count_signed = atoi(argv[++i]);
            if (count_signed < 0)
            {
                printf("error: count must be >= 0\n");
                return -1;
            }
            if (count_signed > MAX_DEPTH)
            {
                printf("error: count too large (max %d)\n", MAX_DEPTH);
                return -1;
            }
            *out_count = (uint32_t) count_signed;
        }
        else
        {
            printf("error: unknown argument '%s'\n", argv[i]);
            return -1;
        }
    }

    if (*out_count == 0)
    {
        printf("error: missing required --count argument\n");
        return -1;
    }

    return 0;
}

int main(int argc, char **argv)
{
    uint32_t count = 0;
    int res = parse_named_args(argc, argv, &count);

    if (res > 0)
    {
        return 0;
    }
    else if (res < 0)
    {
        return 1;
    }

    int current_pid = getpid();
    printf("[pid %d] count=%u\n", current_pid, count);

    if (count == 0)
    {
        printf("[pid %d] leaf reached, exiting.\n", current_pid);
        return 0;
    }

    char next_count_str[16];
    u32_to_str(count - 1, next_count_str);

    char *child_args[] = {
            argv[0],
            "--count", next_count_str,
            NULL
    };

    printf("[pid %d] scheduling child %s --count %s\n",
           current_pid, argv[0], next_count_str);

    sched_add_task(argv[0], 3, child_args, -1);

    printf("[pid %d] spawned child, exiting.\n", current_pid);
    return 0;
}
