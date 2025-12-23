#include <stdint.h>
#include "../../include/kernel/clock.h"
#include "../../include/kernel/panic.h"
#include "../../include/kernel/constants.h"

/* ------------------------------------------------------------
 * I/O helpers
 * ------------------------------------------------------------ */

static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

static inline uint8_t cmos(uint8_t reg)
{
    outb(0x70, 0x80 | reg);
    return inb(0x71);
}

static inline uint8_t bcd(uint8_t v)
{
    return (v & 0x0F) + ((v >> 4) * 10);
}

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ------------------------------------------------------------
 * CPUID
 * ------------------------------------------------------------ */

static uint64_t detect_tsc_hz_or_panic(void)
{
    uint32_t eax, ebx, ecx, edx;

    /* CPUID leaf 0x15 */
    __asm__ volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0x15)
            );

    if (eax && ebx && ecx)
        return (uint64_t)ecx * ebx / eax;

    /* CPUID leaf 0x16 (base frequency MHz) */
    __asm__ volatile(
            "cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(0x16)
            );

    if (eax)
        return (uint64_t)eax * 1000000ULL;

    panic("clock: CPUID does not provide TSC frequency");
    __builtin_unreachable();
}

/* ------------------------------------------------------------
 * RTC (used once)
 * ------------------------------------------------------------ */

static uint32_t rtc_seconds_since_epoch(void)
{
    while (cmos(0x0A) & 0x80) {}

    uint32_t sec  = bcd(cmos(0x00));
    uint32_t min  = bcd(cmos(0x02));
    uint32_t hour = bcd(cmos(0x04));
    uint32_t day  = bcd(cmos(0x07));
    uint32_t mon  = bcd(cmos(0x08));
    uint32_t year = bcd(cmos(0x09));

    uint32_t y = 2000 + year;

    uint32_t days =
            (y - 1970) * 365 +
            (mon - 1) * 30 +
            (day - 1);

    return days * 86400u +
           hour * 3600u +
           min * 60u +
           sec;
}

/* ------------------------------------------------------------
 * Clock state
 * ------------------------------------------------------------ */

static uint32_t boot_epoch_sec;
static uint64_t boot_tsc;
static uint64_t tsc_hz;

/* ------------------------------------------------------------
 * clock_init
 * ------------------------------------------------------------ */

void clock_init(void)
{
    boot_epoch_sec = rtc_seconds_since_epoch();
    boot_tsc = rdtsc();
    tsc_hz = detect_tsc_hz_or_panic();
}

/* ------------------------------------------------------------
 * kclock_gettime
 * ------------------------------------------------------------ */

int kclock_gettime(int clk_id, struct timespec *tp)
{
    if (!tp)
        return -EFAULT;

    uint64_t now = rdtsc();
    uint64_t delta = now - boot_tsc;
    uint64_t ns = (delta * 1000000000ULL) / tsc_hz;

    switch (clk_id)
    {
        case CLOCK_REALTIME:
            tp->tv_sec  = boot_epoch_sec + (uint32_t)(ns / 1000000000ULL);
            tp->tv_nsec = (uint32_t)(ns % 1000000000ULL);
            return 0;

        case CLOCK_MONOTONIC:
        case CLOCK_BOOTTIME:
            tp->tv_sec  = (uint32_t)(ns / 1000000000ULL);
            tp->tv_nsec = (uint32_t)(ns % 1000000000ULL);
            return 0;

        default:
            return -EINVAL;
    }
}
