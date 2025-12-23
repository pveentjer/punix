#include <stdint.h>

#define CLOCK_REALTIME 0
#define EINVAL 22
#define EFAULT 14

struct timespec
{
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

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

/*
 * Crude but safe epoch conversion:
 * - 32-bit only
 * - no division
 * - no modulo
 * - good enough for "current time"
 */
static uint32_t rtc_seconds_since_epoch(void)
{
    while (cmos(0x0A) & 0x80)
    {}

    uint32_t sec = bcd(cmos(0x00));
    uint32_t min = bcd(cmos(0x02));
    uint32_t hour = bcd(cmos(0x04));
    uint32_t day = bcd(cmos(0x07));
    uint32_t mon = bcd(cmos(0x08));
    uint32_t year = bcd(cmos(0x09));

    uint32_t y = 2000 + year;

    // VERY rough day count, intentionally division-free
    uint32_t days =
            (y - 1970) * 365 +
            (mon - 1) * 30 +
            (day - 1);

    return
            days * 86400u +
            hour * 3600u +
            min * 60u +
            sec;
}

int kclock_gettime(int clk_id, struct timespec *tp)
{
    if (!tp)
    {
        return -EFAULT;
    }
    if (clk_id != CLOCK_REALTIME)
    {
        return -EINVAL;
    }

    tp->tv_sec = rtc_seconds_since_epoch();
    tp->tv_nsec = 0;
    return 0;
}
