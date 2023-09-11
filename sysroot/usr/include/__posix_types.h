#if defined(__DECLARE_NULL) && !defined(NULL)
#define NULL ((void *)0)
#endif

#if defined(__DECLARE_SIZE_T) && !defined(__DECLARED_SIZE_T)
typedef unsigned long size_t;
#define __DECLARED_SIZE_T
#endif

#if defined(__DECLARE_SSIZE_T) && !defined(__DECLARED_SSIZE_T)
typedef signed long ssize_t;
#define __DECLARED_SSIZE_T
#endif

#if defined(__DECLARE_OFF_T) && !defined(__DECLARED_OFF_T)
typedef signed long off_t;
#define __DECLARED_OFF_T
#endif


// typedef unsigned long blkcnt_t;
// typedef unsigned long blksize_t;

#if defined(__DECLARE_CLOCK_T) && !defined(__DECLARED_CLOCK_T)
typedef unsigned long clock_t;
#define __DECLARED_CLOCK_T
#endif

#if defined(__DECLARE_CLOCKID_T) && !defined(__DECLARED_CLOCKID_T)
typedef unsigned long clockid_t;
#define __DECLARED_CLOCKID_T
#endif

#if defined(__DECLARE_SUSECONDS_T) && !defined(__DECLARED_SUSECONDS_T)
typedef unsigned long suseconds_t;
#define __DECLARED_SUSECONDS_T
#endif

// typedef unsigned long useconds_t;

#if defined(__DECLARE_TIMER_T) && !defined(__DECLARED_TIMER_T)
typedef unsigned long timer_t;
#define __DECLARED_TIMER_T
#endif


#if defined(__DECLARE_TIME_T) && !defined(__DECLARED_TIME_T)
typedef unsigned long time_t;
#define __DECLARED_TIME_T
#endif

#if defined(__DECLARE_SA_FAMILY_T) && !defined(__DECLARED_SA_FAMILY_T)
// This has to be 64 bits to have the same padding between 32 and 64 bit
// TODO: Revisit this.
typedef unsigned long sa_family_t;
#define __DECLARED_SA_FAMILY_T
#endif

// typedef unsigned long dev_t;

// typedef unsigned long fsblkcnt_t;
// typedef unsigned long fsfilcnt_t;

#if defined(__DECLARE_ID_T) && !defined(__DECLARED_ID_T)
typedef unsigned long id_t;
#define __DECLARED_ID_T
#endif

#if defined(__DECLARE_GID_T) && !defined(__DECLARED_GID_T)
typedef unsigned long gid_t;
#define __DECLARED_GID_T
#endif

#if defined(__DECLARE_UID_T) && !defined(__DECLARED_UID_T)
typedef unsigned long uid_t;
#define __DECLARED_UID_T
#endif

#if defined(__DECLARE_PID_T) && !defined(__DECLARED_PID_T)
typedef unsigned long pid_t;
#define __DECLARED_PID_T
#endif

#if defined(__DECLARE_IDTYPE_T) && !defined(__DECLARED_IDTYPE_T)
typedef unsigned long idtype_t;
#define __DECLARED_IDTYPE_T
#endif

#if defined(__DECLARE_INO_T) && !defined(__DECLARED_INO_T)
typedef unsigned long ino_t;
#define __DECLARED_INO_T
#endif

// typedef unsigned long key_t;

// typedef unsigned long mode_t;
// typedef unsigned long nlink_t;

#if defined(__DECLARE_PTHREAD_ATTR_T) && !defined(__DECLARED_PTHREAD_ATTR_T)
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
// typedef unsigned long pthread_key_t;
// typedef volatile void * pthread_mutex_t;
// typedef unsigned long pthread_mutexattr_t;
// typedef volatile unsigned pthread_once_t;
// typedef struct pthread_rwlock_t {
//     pthread_cond_t cond;
//     pthread_mutex_t g;

//     unsigned long num_readers_active;
//     unsigned long num_writers_waiting;
//     unsigned long writer_active;
// } pthread_rwlock_t;
// typedef unsigned long pthread_rwlockattr_t;
#if defined(__DECLARE_PTHREAD_T) && !defined(__DECLARED_PTHREAD_T)
typedef unsigned long pthread_t;
#define __DECLARED_PTHREAD_T
#endif

#if defined(__DECLARE_STACK_T) && !defined(__DECLARED_STACK_T)
typedef struct stack_t {
    void     *ss_sp;
    unsigned long    ss_size;
    int       ss_flags;
} stack_t;
#define __DECLARED_STACK_T
#endif

#if defined(__DECLARE_LOCALE_T) && !defined(__DECLARED_LOCALE_T)
typedef unsigned long locale_t;
#define __DECLARED_LOCALE_T
#endif