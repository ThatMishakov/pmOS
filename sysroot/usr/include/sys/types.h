#ifndef _SYS_TYPES_H
#define _SYS_TYPES_H
#include "../stdlib_com.h"

typedef unsigned long _u64;
typedef unsigned int  _u32;

typedef size_t ssize_t;

// typedef size_t off_t;
// typedef size_t mode_t;

typedef size_t blkcnt_t;
typedef size_t blksize_t;

typedef _u64 clock_t;
typedef _u64 clockid_t;
typedef _u64 suseconds_t;
typedef _u64 useconds_t;
typedef _u64 timer_t;


typedef _u64 dev_t;

typedef size_t fsblkcnt_t;
typedef size_t fsfilcnt_t;

typedef _u64 id_t;
typedef id_t gid_t;
typedef id_t uid_t;
typedef id_t pid_t;

typedef size_t ino_t;

typedef _u64 key_t;

typedef size_t mode_t;
typedef size_t nlink_t;
typedef size_t off_t;

typedef struct {
    unsigned long stackaddr;
    unsigned long stacksize;
    unsigned long guardsize;
    
    unsigned char scope;
    unsigned char detachstate;
    unsigned char inheritsched;
    unsigned char schedpolicy;
} pthread_attr_t;

typedef volatile void * pthread_cond_t;
typedef unsigned pthread_condattr_t;
typedef _u64 pthread_key_t;
typedef volatile void * pthread_mutex_t;
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
typedef _u64 pthread_t;


#endif