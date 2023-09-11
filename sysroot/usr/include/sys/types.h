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
#include "../__posix_types.h"

typedef unsigned long _u64;
typedef unsigned int  _u32;

typedef signed long _i64;

// typedef size_t off_t;
// typedef size_t mode_t;

typedef size_t blkcnt_t;
typedef size_t blksize_t;

typedef _u64 clock_t;
typedef _u64 clockid_t;
typedef _u64 suseconds_t;
typedef _u64 useconds_t;
typedef _u64 timer_t;
typedef _u64 time_t;


typedef _u64 dev_t;

typedef size_t fsblkcnt_t;
typedef size_t fsfilcnt_t;

typedef size_t ino_t;

typedef _u64 key_t;

typedef size_t mode_t;
typedef size_t nlink_t;
typedef ssize_t off_t;

typedef volatile void * pthread_cond_t;
typedef unsigned pthread_condattr_t;
typedef _u64 pthread_key_t;

struct __pthread_waiter {
    _u64 notification_port;
    struct __pthread_waiter* next;
};

typedef struct {
    _u64 block_count;
    _u64 blocking_thread_id;
    struct __pthread_waiter* waiters_list_head;
    struct __pthread_waiter* waiters_list_tail;
    _u64 recursive_lock_count;
    int type;
} pthread_mutex_t;

typedef size_t pthread_mutexattr_t;
typedef volatile unsigned pthread_once_t;
typedef struct pthread_rwlock_t {
    pthread_cond_t cond;
    pthread_mutex_t g;

    size_t num_readers_active;
    size_t num_writers_waiting;
    size_t writer_active;
} pthread_rwlock_t;
typedef size_t pthread_rwlockattr_t;


#endif