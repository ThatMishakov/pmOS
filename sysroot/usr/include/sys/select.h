#ifndef _SYS_SELECT_H
#define _SYS_SELECT_H 1

#include <time.h>
#include <signal.h>

struct timeval {
    time_t tv_sec; //< Seconds
    suseconds_t tv_usec; //< Microseconds
};

#define FD_SETSIZE 1024

typedef struct {
    unsigned long fds_bits[FD_SETSIZE / (8 * sizeof(long))];
} fd_set;

#ifdef __STDC_HOSTED__

#ifdef __cplusplus
extern "C" {
#endif

void FD_CLR(int, fd_set *);
int  FD_ISSET(int, fd_set *);
void FD_SET(int, fd_set *);
void FD_ZERO(fd_set *);

int  pselect(int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
         const struct timespec *restrict, const sigset_t *restrict);
int  select(int, fd_set *restrict, fd_set *restrict, fd_set *restrict,
         struct timeval *restrict);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // __STDC_HOSTED__

#endif // _SYS_SELECT_H