typedef unsigned long _u64;
typedef unsigned int  _u32;

#define __DECLARE_SIZE_T \
typedef _u64 size_t;

#define __DECLARE_SSIZE_T \
typedef _u64 ssize_t;


// typedef size_t blkcnt_t;
// typedef size_t blksize_t;

typedef _u64 clock_t;
// typedef _u64 clockid_t;
// typedef _u64 suseconds_t;
// typedef _u64 useconds_t;
// typedef _u64 timer_t;

#if defined(__DECLARE_TIME_T) && ! defined(__DECLARED_TIME_T)
typedef _u64 time_t;
#define __DECLARED_TIME_T
#endif

#if defined(__DECLARE_SA_FAMILY_T) && !defined(__DECLARED_SA_FAMILY_T)
// This has to be 64 bits to have the same padding between 32 and 64 bit
// TODO: Revisit this.
typedef _u64 sa_family_t;
#define __DECLARED_SA_FAMILY_T
#endif

// typedef _u64 dev_t;

// typedef size_t fsblkcnt_t;
// typedef size_t fsfilcnt_t;

#if defined (__DECLARE_ID_T) && ! defined (__DECLARED_ID_T)
typedef _u64 id_t;
#define __DECLARED_ID_T
#endif

#if defined (__DECLARE_GID_T) && ! defined (__DECLARED_GID_T)
typedef _u64 gid_t;
#define __DECLARED_GID_T
#endif

#if defined (__DECLARE_UID_T) && ! defined (__DECLARED_UID_T)
typedef _u64 uid_t;
#define __DECLARED_UID_T
#endif

#if defined (__DECLARE_PID_T) && ! defined (__DECLARED_PID_T)
typedef _u64 pid_t;
#define __DECLARED_PID_T
#endif

#if defined (__DECLARE_IDTYPE_T) && ! defined (__DECLARED_IDTYPE_T)
typedef _u64 idtype_t;
#define __DECLARED_IDTYPE_T
#endif

#if defined (__DECLARE_INO_T) && defined (__DECLARED_INO_T)
typedef size_t ino_t;
#define __DECLARED_INO_T
#endif

// typedef _u64 key_t;

// typedef size_t mode_t;
// typedef size_t nlink_t;
// typedef size_t off_t;

#if defined  (__DECLARE_PTHREAD_ATTR_T) && ! defined (__DECLARED_PTHREAD_ATTR_T)
typedef struct {
    unsigned long stackaddr;
    unsigned long stacksize;
    unsigned long guardsize;

    unsigned char scope;
    unsigned char detachstate;
    unsigned char inheritsched;
    unsigned char schedpolicy;
} pthread_attr_t;
#define __DECLARED_PTHREAD_ATTR_T
#endif

// typedef volatile void * pthread_cond_t;
// typedef unsigned pthread_condattr_t;
// typedef _u64 pthread_key_t;
// typedef volatile void * pthread_mutex_t;
// typedef size_t pthread_mutexattr_t;
// typedef volatile unsigned pthread_once_t;
// typedef struct pthread_rwlock_t {
//     pthread_cond_t cond;
//     pthread_mutex_t g;

//     size_t num_readers_active;
//     size_t num_writers_waiting;
//     size_t writer_active;
// } pthread_rwlock_t;
// typedef size_t pthread_rwlockattr_t;
#if defined  (__DECLARE_PTHREAD_T) && ! defined (__DECLARED_PTHREAD_T)
typedef _u64 pthread_t;
#define __DECLARED_PTHREAD_T
#endif

#if defined(__DECLARE_STACK_T) && !defined(__DECLARED_STACK_T)
typedef struct stack_t {
    void     *ss_sp;
    size_t    ss_size;
    int       ss_flags;
} stack_t;
#define __DECLARED_STACK_T
#endif