#ifndef TIME_H
#define TIME_H

#include <stdint.h>

typedef int32_t clockid_t;

struct timespec
{
    int64_t tv_sec;
    int64_t tv_nsec;
};

#define CLOCK_REALTIME   0
#define CLOCK_MONOTONIC  1

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif
