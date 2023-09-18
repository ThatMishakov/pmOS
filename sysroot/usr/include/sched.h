#ifndef _SCHED_H
#define _SCHED_H 1

#define __DECLARE_PID_T
#define __DECLARE_TIME_T
#include "__posix_types.h"

#include <time.h>

struct sched_param {
    int sched_priority; //< Process execution scheduling priority.
    int sched_ss_low_priority; //< Low scheduling priority for sporadic server.
    struct timespec sched_ss_repl_period; //< Replenishment period for sporadic server.
    struct timespec sched_ss_init_budget; //< Initial budget for sporadic server.
    int sched_ss_max_repl; //< Maximum pending replenishments for sporadic server.
};

/// Scheduling policies constants
enum {
    SCHED_FIFO = 0, //< First in, first out scheduling policy.
    SCHED_RR = 1, //< Round robin scheduling policy.
    SCHED_SPORADIC = 2, //< Sporadic server scheduling policy.
    SCHED_OTHER = 3, //< Another scheduling policy.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int    sched_get_priority_max(int);
int    sched_get_priority_min(int);

int    sched_getparam(pid_t, struct sched_param *);
int    sched_getscheduler(pid_t);

int    sched_rr_get_interval(pid_t, struct timespec *);

int    sched_setparam(pid_t, const struct sched_param *);
int    sched_setscheduler(pid_t, int, const struct sched_param *);

int    sched_yield(void);

#endif // __STDC_HOSTED__

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif // _SCHED_H