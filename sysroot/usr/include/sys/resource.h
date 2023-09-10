#ifdef _SYS_RESOURCE_H
#define _SYS_RESOURCE_H

#define __DECLARE_ID_T
#include "__posix_types.h"

enum {
    PRIO_PROCESS = 0,
    PRIO_PGRP = 1,
    PRIO_USER = 2,
};

/// Type used for limit values.
typedef unsigned long rlim_t;

/// A value for `rlim_t` representing no limit.
#define RLIM_INFINITY ((rlim_t)-1)

/// A value for `rlim_t` representing unrepresentable saved hard limit.
#define RLIM_SAVED_MAX RLIM_INFINITY

/// A value for `rlim_t` representing unrepresentable hard limit.
#define RLIM_SAVED_CUR RLIM_INFINITY


enum {
    RUSAGE_SELF = 0, //< The calling process.
    RUSAGE_CHILDREN = -1, //< All children processes.
};

struct rlimit {
    rlim_t rlim_cur; //< The current (soft) limit.
    rlim_t rlim_max; //< The hard limit.
};

struct rusage {
    struct timeval ru_utime; //< User time used.
    struct timeval ru_stime; //< System time used.
};

enum {
    RLIMIT_CORE = 0, //< Maximum size of core file.
    RLIMIT_CPU = 1, //< CPU time limit in seconds.
    RLIMIT_DATA = 2, //< Maximum size of data segment.
    RLIMIT_FSIZE = 3, //< Maximum size of files created.
    RLIMIT_NOFILE = 4, //< Maximum number of open files.
    RLIMIT_STACK = 5, //< Maximum size of stack segment.
    RLIMIT_AS = 6, //< Address space limit.
    RLIMIT_NPROC = 7, //< Maximum number of processes.
    RLIMIT_MEMLOCK = 8, //< Maximum locked-in-memory address space.
    RLIMIT_LOCKS = 9, //< Maximum number of file locks.
    RLIMIT_SIGPENDING = 10, //< Maximum number of pending signals.
    RLIMIT_MSGQUEUE = 11, //< Maximum bytes in POSIX message queues.
    RLIMIT_NICE = 12, //< Maximum nice priority allowed to raise to.
    RLIMIT_RTPRIO = 13, //< Maximum realtime priority allowed for non-priviledged processes.
    RLIMIT_RTTIME = 14, //< Maximum CPU time in microseconds that a process scheduled under a real-time scheduling policy may consume without making a blocking system call.
};

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __STDC_HOSTED__

int  getpriority(int, id_t);
int  getrlimit(int, struct rlimit *);
int  getrusage(int, struct rusage *);
int  setpriority(int, id_t, int);
int  setrlimit(int, const struct rlimit *);

#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif