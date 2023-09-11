#ifndef _SYS_TIME_H
#define _SYS_TIME_H

#include <sys/select.h>

struct itimerval {
    struct timeval it_interval; //< Interval for periodic timer
    struct timeval it_value; //< Time until next expiration
};

enum {
    ITIMER_REAL = 0, //< Decrements in real time
    ITIMER_VIRTUAL = 1, //< Decrements in process virtual time
    ITIMER_PROF = 2, //< Decrements both in process virtual time and when the system is running on behalf of the process
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int   getitimer(int, struct itimerval *);
int   gettimeofday(struct timeval *, void *);
int   setitimer(int, const struct itimerval *,
          struct itimerval *);
int   select(int, fd_set *, fd_set *, fd_set *,
          struct timeval *);
int   utimes(const char *, const struct timeval [2]);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SYS_TIME_H