#include "libc.h"
#include "time.h"
#include "string.h"
#include <stdbool.h>
#include <stdint.h>

/* ------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------ */

static const char *WEEKDAY[] = {
        "Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};

static const char *MONTH[] = {
        "Jan","Feb","Mar","Apr","May","Jun",
        "Jul","Aug","Sep","Oct","Nov","Dec"
};

static bool is_leap(int y)
{
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

static int days_in_month(int y, int m)
{
    static const int dim[] = {
            31,28,31,30,31,30,31,31,30,31,30,31
    };

    if (m == 1 && is_leap(y))
        return 29;

    return dim[m];
}

/*
 * Convert seconds since epoch to UTC date
 * 32-bit ONLY — no __divmoddi4
 */
static void epoch_to_utc(uint32_t t,
                         int *year, int *mon, int *mday,
                         int *hour, int *min, int *sec,
                         int *wday)
{
    *sec  = t % 60;  t /= 60;
    *min  = t % 60;  t /= 60;
    *hour = t % 24;  t /= 24;

    *wday = (int)((t + 4) % 7);  /* 1970-01-01 was Thu */

    if ((unsigned)*wday >= 7u)
    {
        printf("wday out of range in epoch_to_utc\n");
    }


    int y = 1970;
    while (1)
    {
        int days = is_leap(y) ? 366 : 365;
        if (t < (uint32_t)days) break;
        t -= days;
        y++;
    }

    int m = 0;
    while (1)
    {
        int d = days_in_month(y, m);
        if (t < (uint32_t)d) break;
        t -= d;
        m++;
    }

    *year = y;
    *mon  = m;
    *mday = (int)t + 1;
}

/* ------------------------------------------------------------
 * date command
 * ------------------------------------------------------------ */

static void usage(void)
{
    printf("usage: date [--help]\n");
}

int main(int argc, char **argv)
{
    if (argc > 1)
    {
        usage();
        return strcmp(argv[1], "--help") == 0 ? 0 : 1;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        printf("date: clock_gettime failed\n");
        return 1;
    }

    int year, mon, mday, hour, min, sec, wday;
    epoch_to_utc((uint32_t)ts.tv_sec,
                 &year,&mon,&mday,
                 &hour,&min,&sec,
                 &wday);

    printf("%s %s %02d %02d:%02d:%02d UTC %d\n",
           WEEKDAY[wday],
           MONTH[mon],
           mday,
           hour,min,sec,
           year);

    return 0;
}
