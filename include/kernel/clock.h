//
// Created by pveentjer on 12/23/25.
//

#ifndef KERNEL_CLOCK_H
#define KERNEL_CLOCK_H

#include "time.h"

/*
 * Kernel-internal clock access.
 *
 * Fills a timespec with the current time for the given clock.
 * Only CLOCK_REALTIME is required initially.
 */
int kclock_gettime(clockid_t clk_id, struct timespec *ts);

#endif // KERNEL_CLOCK_H
