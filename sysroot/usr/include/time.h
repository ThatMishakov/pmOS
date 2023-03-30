#ifndef __TIME_H
#define __TIME_H 1
#include "stdlib_com.h"

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