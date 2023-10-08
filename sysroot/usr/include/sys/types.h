#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H

#define __DECLARE_PTHREAD_T
#define __DECLARE_PTHREAD_ATTR_T
#define __DECLARE_SIZE_T
#define __DECLARE_SSIZE_T
#define __DECLARE_ID_T
#define __DECLARE_PID_T
#define __DECLARE_GID_T
#define __DECLARE_UID_T
#define __DECLARE_CLOCK_T
#define __DECLARE_CLOCKID_T
#define __DECLARE_SUSECONDS_T
#define __DECLARE_USECONDS_T
#define __DECLARE_TIMER_T
#define __DECLARE_TIME_T
#define __DECLARE_BLKCNT_T
#define __DECLARE_BLKSIZE_T
#define __DECLARE_OFF_T
#define __DECLARE_DEV_T
#define __DECLARE_PTHREAD_MUTEX_T
#define __DECLARE_MODE_T
#define __DECLARE_PTHREAD_COND_T
#include "../__posix_types.h"

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

typedef unsigned long ino_t;

typedef unsigned long key_t;

typedef unsigned long nlink_t;

typedef unsigned pthread_condattr_t;
typedef struct {
    size_t index;
    size_t generation;
} pthread_key_t;

typedef unsigned long pthread_mutexattr_t;

typedef volatile unsigned pthread_once_t;

typedef struct pthread_rwlock_t {
    pthread_cond_t cond;
    pthread_mutex_t g;

    unsigned long num_readers_active;
    unsigned long num_writers_waiting;
    unsigned long writer_active;
} pthread_rwlock_t;

typedef unsigned long pthread_rwlockattr_t;


#endif