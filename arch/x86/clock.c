#include <stdint.h>

/* ------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------ */

#define CLOCK_REALTIME   0
#define CLOCK_MONOTONIC  1
#define CLOCK_BOOTTIME   2

#define EINVAL 22
#define EFAULT 14

struct timespec
{
    uint32_t tv_sec;
    uint32_t tv_nsec;
};

void panic(const char *msg);

void kprintf(const char *fmt, ...);

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

/* ------------------------------------------------------------
 * Serialized RDTSC
 * ------------------------------------------------------------ */

static inline uint64_t rdtsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile(
            "lfence\n\t"
            "rdtsc"
            : "=a"(lo), "=d"(hi)
            :
            : "memory"
            );
    return ((uint64_t) hi << 32) | lo;
}

/* ------------------------------------------------------------
 * CPUID helpers
 * ------------------------------------------------------------ */

static inline void cpuid(uint32_t leaf,
                         uint32_t *a, uint32_t *b,
                         uint32_t *c, uint32_t *d)
{
    __asm__ volatile(
            "cpuid"
            : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
            : "a"(leaf)
            );
}

static int running_under_hypervisor(void)
{
    uint32_t a, b, c, d;
    cpuid(1, &a, &b, &c, &d);
    return (c & (1u << 31)) != 0;
}

static void require_reasonable_tsc(void)
{
    uint32_t a, b, c, d;
    cpuid(0x80000007, &a, &b, &c, &d);

    /* invariant TSC */
    if (d & (1u << 8))
    {
        return;
    }

    /* hypervisor (QEMU/KVM/etc) */
    if (running_under_hypervisor())
    {
        return;
    }

    panic("clock: non-invariant TSC on bare metal");
}

/* ------------------------------------------------------------
 * RTC helpers (for epoch only)
 * ------------------------------------------------------------ */

static int rtc_is_bcd(void)
{
    return (cmos(0x0B) & 0x04) == 0;
}

static uint8_t rtc_value(uint8_t reg)
{
    uint8_t v = cmos(reg);
    return rtc_is_bcd() ? bcd(v) : v;
}

static uint32_t rtc_seconds_since_epoch(void)
{
    while (cmos(0x0A) & 0x80)
    {}

    uint32_t sec = rtc_value(0x00);
    uint32_t min = rtc_value(0x02);
    uint32_t hour = rtc_value(0x04);
    uint32_t day = rtc_value(0x07);
    uint32_t mon = rtc_value(0x08);
    uint32_t year = rtc_value(0x09);

    uint32_t y = 2000 + year;

    /* Days per month (non-leap year) */
    static const uint32_t days_before[] = {
            0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };

    uint32_t days = (y - 1970) * 365 +
                    ((y - 1969) / 4) +
                    days_before[mon - 1] +
                    (day - 1);

    /* Adjust for leap year if past February */
    if (mon > 2 && (y % 4 == 0) && ((y % 100 != 0) || (y % 400 == 0)))
    {
        days++;
    }

    return days * 86400u + hour * 3600u + min * 60u + sec;
}

/* ------------------------------------------------------------
 * PIT (Programmable Interval Timer)
 * ------------------------------------------------------------ */

#define PIT_FREQUENCY 1193182ULL
#define PIT_CH2_DATA  0x42
#define PIT_MODE      0x43
#define PIT_SPKR      0x61

static uint64_t calibrate_tsc_pit(void)
{
    /* Use channel 2 (speaker) in one-shot mode */
    outb(PIT_MODE, 0xB0);

    /* Set count for ~50ms: 1193182 / 20 = 59659 */
    uint16_t count = 59659;
    outb(PIT_CH2_DATA, count & 0xFF);
    outb(PIT_CH2_DATA, count >> 8);

    /* Enable gate, disable speaker */
    uint8_t spkr = inb(PIT_SPKR);
    outb(PIT_SPKR, (spkr & 0xFC) | 0x01);

    /* Small delay for PIT to start */
    for (volatile int i = 0; i < 1000; i++);

    uint64_t t0 = rdtsc();

    /* Wait for countdown to complete (bit 5 goes high) */
    while (!(inb(PIT_SPKR) & 0x20))
    {}

    uint64_t t1 = rdtsc();

    /* Restore speaker state */
    outb(PIT_SPKR, spkr);

    return (t1 - t0) * PIT_FREQUENCY / count;
}

/* ------------------------------------------------------------
 * Clock state
 * ------------------------------------------------------------ */

static uint32_t boot_epoch_sec;
static uint64_t boot_tsc;
static uint64_t tsc_hz;
static uint64_t last_ns;

/* ------------------------------------------------------------
 * clock_init
 * ------------------------------------------------------------ */

void clock_init(void)
{
    require_reasonable_tsc();

    boot_epoch_sec = rtc_seconds_since_epoch();
    tsc_hz = calibrate_tsc_pit();
    boot_tsc = rdtsc();
    last_ns = 0;
}

/* ------------------------------------------------------------
 * kclock_gettime
 * ------------------------------------------------------------ */

int kclock_gettime(int clk_id, struct timespec *tp)
{
    if (!tp)
    {
        return -EFAULT;
    }

    uint64_t now = rdtsc();
    uint64_t delta = now - boot_tsc;

    /* Avoid overflow by doing division first */
    uint64_t sec = delta / tsc_hz;
    uint64_t rem = delta % tsc_hz;
    uint64_t ns = sec * 1000000000ULL + (rem * 1000000000ULL) / tsc_hz;

    /* monotonic clamp */
    if (ns < last_ns)
    {
        ns = last_ns;
    }
    else
    {
        last_ns = ns;
    }

    switch (clk_id)
    {
        case CLOCK_REALTIME:
            tp->tv_sec = boot_epoch_sec + (uint32_t) (ns / 1000000000ULL);
            tp->tv_nsec = (uint32_t) (ns % 1000000000ULL);
            return 0;
        case CLOCK_MONOTONIC:
        case CLOCK_BOOTTIME:
            tp->tv_sec = (uint32_t) (ns / 1000000000ULL);
            tp->tv_nsec = (uint32_t) (ns % 1000000000ULL);
            return 0;
        default:
            return -EINVAL;
    }
}