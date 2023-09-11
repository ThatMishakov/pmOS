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
#include "../__posix_types.h"

typedef size_t dev_t;

typedef size_t fsblkcnt_t;
typedef size_t fsfilcnt_t;

typedef size_t ino_t;

typedef size_t key_t;

typedef size_t mode_t;
typedef size_t nlink_t;
typedef ssize_t off_t;

typedef volatile void * pthread_cond_t;
typedef unsigned pthread_condattr_t;
typedef unsigned long pthread_key_t;

struct __pthread_waiter {
    unsigned long notification_port;
    struct __pthread_waiter* next;
};

typedef struct {
    unsigned long block_count;
    unsigned long blocking_thread_id;
    struct __pthread_waiter* waiters_list_head;
    struct __pthread_waiter* waiters_list_tail;
    unsigned long recursive_lock_count;
    int type;
} pthread_mutex_t;

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