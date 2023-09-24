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
#define __DECLARE_TIMESPEC
#define __DECLARE_TIME_T
#define __DECLARE_CLOCK_T
#define __DECLARE_NULL
#include "__posix_types.h"

extern int    daylight;
extern long   timezone;
extern char  *tzname[];

#if defined(__cplusplus)
extern "C" {
#endif

unsigned long pmos_clocks_per_sec(void);

/// @brief The number of clock ticks per second.
#define CLOCKS_PER_SEC pmos_clocks_per_sec()
// #define TIME_UTC 0

/// Monotonic system-wide clock.
#define CLOCK_MONOTONIC 0

/// The CPU-time clock of the calling process.
#define CLOCK_PROCESS_CPUTIME_ID 1

/// The system-wide clock that measures real (i.e., wall-clock) time.
#define CLOCK_REALTIME 2

/// The CPU-time clock of the calling thread.
#define CLOCK_THREAD_CPUTIME_ID 3

/// Flag indicating time is absolute.
#define TIMER_ABSTIME 1

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
char      *asctime_r(const struct tm *, char *);
char *ctime(const time_t *timer);
char      *ctime_r(const time_t *, char *);
struct tm *gmtime(const time_t *timer);
struct tm *gmtime_r(const time_t *, struct tm *);
struct tm *localtime(const time_t *timer);
struct tm *localtime_r(const time_t *, struct tm *);
size_t strftime(char * s,
                size_t maxsize,
                const char * format,
                const struct tm * timeptr);
size_t     strftime_l(char *, size_t, const char *,
               const struct tm *, locale_t);

int        clock_getres(clockid_t, struct timespec *);
int        clock_gettime(clockid_t, struct timespec *);
int        clock_nanosleep(clockid_t, int, const struct timespec *,
               struct timespec *);
int        clock_settime(clockid_t, const struct timespec *);

int        clock_getcpuclockid(pid_t, clockid_t *);

double     difftime(time_t, time_t);
struct tm *getdate(const char *);


int        nanosleep(const struct timespec *, struct timespec *);
char      *strptime(const char *, const char *,
               struct tm *);

int        timer_create(clockid_t, struct sigevent *,
               timer_t *);
int        timer_delete(timer_t);
int        timer_getoverrun(timer_t);
int        timer_gettime(timer_t, struct itimerspec *);
int        timer_settime(timer_t, int, const struct itimerspec *,
               struct itimerspec *);
void       tzset(void);


#endif


#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif