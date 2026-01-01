#include "time.h"
#include "stdio.h"
#include "string.h"
#include <stdint.h>

static void print_help(void)
{
    printf("Usage: uptime [OPTION]\n");
    printf("Show how long the system has been running.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help     display this help and exit\n");
}

int main(int argc, char **argv)
{
    /* Check for help flag */
    if (argc > 1)
    {
        if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)
        {
            print_help();
            return 0;
        }
        else
        {
            printf("uptime: invalid option '%s'\n", argv[1]);
            printf("Try 'uptime --help' for more information.\n");
            return 1;
        }
    }

    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        printf("uptime: clock_gettime failed\n");
        return 1;
    }

    uint32_t total = ts.tv_sec;

    uint32_t days = total / 86400;
    total %= 86400;
    uint32_t hours = total / 3600;
    total %= 3600;
    uint32_t mins = total / 60;
    uint32_t secs = total % 60;

    if (days)
    {
        printf("up %ud %02u:%02u:%02u\n", days, hours, mins, secs);
    }
    else
    {
        printf("up %02u:%02u:%02u\n", hours, mins, secs);
    }

    return 0;
}