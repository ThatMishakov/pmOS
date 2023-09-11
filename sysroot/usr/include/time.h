#ifndef __TIME_H
#define __TIME_H 1
#include <signal.h>

#define __DECLARE_CLOCK_T
#define __DECLARE_TIME_T
#define __DECLARE_SIZE_T
#define __DECLARE_PID_T
#define __DECLARE_CLOCKID_T
#define __DECLARE_TIMER_T
#define __DECLARE_LOCALE_T
#define __DECLARE_NULL
#include "__posix_types.h"

extern int    daylight;
extern long   timezone;
extern char  *tzname[];

#if defined(__cplusplus)
extern "C" {
#endif

#define CLOCKS_PER_SEC 1000
// #define TIME_UTC 0

typedef unsigned long clock_t;
typedef clock_t time_t;

typedef struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

typedef struct tm {
    int tm_sec;
    int tm_min; 
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst; 
} tm;

#ifdef __STDC_HOSTED__
/* Time manipulation */
clock_t clock(void);
double difftime(time_t time1, time_t time0);
time_t mktime(struct tm *timeptr);

/**
 * @brief Get the current calendar time.
 *
 * The `time` function returns the current calendar time as the number of seconds
 * elapsed since the epoch (January 1, 1970, 00:00:00 UTC).
 *
 * @param timer Pointer to a `time_t` object where the current time will be stored.
 * @return The current calendar time as the number of seconds since the epoch. If an
 *         error occurred, the function returns `(time_t)-1`.
 */
time_t time(time_t *timer);

int timespec_get(struct timespec *ts, int base);

/* Time conversion */
char *asctime(const struct tm *timeptr);
char *ctime(const time_t *timer);
struct tm *gmtime(const time_t *timer);
struct tm *localtime(const time_t *timer);
size_t strftime(char * s,
                size_t maxsize,
                const char * format,
                const struct tm * timeptr);

#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif