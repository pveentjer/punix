#include "time.h"
#include "stdio.h"
#include <stdint.h>

int main(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        printf("uptime: clock_gettime failed\n");
        return 1;
    }

    uint32_t total = ts.tv_sec;

    uint32_t days  = total / 86400; total %= 86400;
    uint32_t hours = total / 3600;  total %= 3600;
    uint32_t mins  = total / 60;
    uint32_t secs  = total % 60;

    if (days)
        printf("up %ud %02u:%02u:%02u\n", days, hours, mins, secs);
    else
        printf("up %02u:%02u:%02u\n", hours, mins, secs);

    return 0;
}
