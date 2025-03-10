/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__DECLARE_WCHAR_T) && !defined(__DECLARED_WCHAR_T)
    // wchar_t is a keyword in C++
    #ifndef __cplusplus
typedef int wchar_t;
    #endif
    #define __DECLARED_WCHAR_T
#endif

#if defined(__DECLARE_NULL) && !defined(__DECLARED_NULL)
    #ifndef __cplusplus
        #define NULL ((void *)0)
    #else
        // Hooray: C++ defines NULL as 0 or nullptr, and C defines it as (void *)0
        #define NULL 0
    #endif
    #define __DECLARED_NULL
#endif

#if defined(__DECLARE_SIZE_T) && !defined(__DECLARED_SIZE_T)
// && !defined(__SIZE_TYPE__) // GCC stddef.h craziness
#ifdef __i386__
typedef unsigned int size_t;
#else
typedef unsigned long size_t;
#endif
#define __DECLARED_SIZE_T
#endif

#if defined(__DECLARE_SSIZE_T) && !defined(__DECLARED_SSIZE_T)
typedef long ssize_t;
#define __DECLARED_SSIZE_T
#endif

#if defined(__DECLARE_PTRDIFF_T) && !defined(__DECLARED_PTRDIFF_T)
#ifdef __i386__
typedef int ptrdiff_t;
#else
typedef long ptrdiff_t;
#endif
#define __DECLARED_PTRDIFF_T
#endif

#ifndef __PMOS64__
#ifdef __i386__
typedef long long __pmos64_t;
typedef unsigned long long __pmos64u_t;
typedef unsigned __pmos32u_t;
#else
typedef long __pmos64_t;
typedef unsigned long __pmos64u_t;
typedef unsigned __pmos32u_t;
#endif
#define __PMOS64__
#endif


#if defined(__DECLARE_OFF_T) && !defined(__DECLARED_OFF_T)
typedef __pmos64_t off_t;
    #define __DECLARED_OFF_T
#endif

#if defined(__DECLARE_BLKCNT_T) && !defined(__DECLARED_BLKCNT_T)
typedef __pmos64u_t blkcnt_t;
    #define __DECLARED_BLKCNT_T
#endif

#if defined(__DECLARE_BLKSIZE_T) && !defined(__DECLARED_BLKSIZE_T)
typedef __pmos32u_t blksize_t;
    #define __DECLARED_BLKSIZE_T
#endif

#if defined(__DECLARE_CLOCK_T) && !defined(__DECLARED_CLOCK_T)
typedef __pmos64u_t clock_t;
    #define __DECLARED_CLOCK_T
#endif

#if defined(__DECLARE_CLOCKID_T) && !defined(__DECLARED_CLOCKID_T)
typedef __pmos64u_t clockid_t;
    #define __DECLARED_CLOCKID_T
#endif

#if defined(__DECLARE_SUSECONDS_T) && !defined(__DECLARED_SUSECONDS_T)
typedef __pmos64u_t suseconds_t;
    #define __DECLARED_SUSECONDS_T
#endif

#if defined(__DECLARE_USECONDS_T) && !defined(__DECLARED_USECONDS_T)
typedef __pmos64u_t useconds_t;
    #define __DECLARED_USECONDS_T
#endif

#if defined(__DECLARE_TIMER_T) && !defined(__DECLARED_TIMER_T)
typedef __pmos64u_t timer_t;
    #define __DECLARED_TIMER_T
#endif

#if (defined(__DECLARE_TIME_T) || defined(__DECLARE_TIMESPEC_T)) && !defined(__DECLARED_TIME_T)
typedef __pmos64_t time_t;
    #define __DECLARED_TIME_T
#endif

#if defined(__DECLARE_SA_FAMILY_T) && !defined(__DECLARED_SA_FAMILY_T)
// This has to be 64 bits to have the same padding between 32 and 64 bit
// TODO: Revisit this.
typedef unsigned short sa_family_t;
    #define __DECLARED_SA_FAMILY_T
#endif

#if defined(__DECLARE_DEV_T) && !defined(__DECLARED_DEV_T)
typedef __pmos64u_t dev_t;
    #define __DECLARED_DEV_T
#endif

// typedef unsigned long fsblkcnt_t;
// typedef unsigned long fsfilcnt_t;

#if defined(__DECLARE_ID_T) && !defined(__DECLARED_ID_T)
typedef __pmos64u_t id_t;
    #define __DECLARED_ID_T
#endif

#if defined(__DECLARE_GID_T) && !defined(__DECLARED_GID_T)
typedef __pmos64u_t gid_t;
    #define __DECLARED_GID_T
#endif

#if defined(__DECLARE_UID_T) && !defined(__DECLARED_UID_T)
typedef __pmos64u_t uid_t;
    #define __DECLARED_UID_T
#endif

#if defined(__DECLARE_PID_T) && !defined(__DECLARED_PID_T)
typedef __pmos64_t pid_t;
    #define __DECLARED_PID_T
#endif

#if defined(__DECLARE_IDTYPE_T) && !defined(__DECLARED_IDTYPE_T)
typedef __pmos64u_t idtype_t;
    #define __DECLARED_IDTYPE_T
#endif

#if defined(__DECLARE_INO_T) && !defined(__DECLARED_INO_T)
typedef __pmos64u_t ino_t;
    #define __DECLARED_INO_T
#endif

// typedef unsigned long key_t;

#if defined(__DECLARE_MODE_T) && !defined(__DECLARED_MODE_T)
typedef __pmos32u_t mode_t;
    #define __DECLARED_MODE_T
#endif

#if defined(__DECLARE_NLINK_T) && !defined(__DECLARED_NLINK_T)
typedef __pmos64u_t nlink_t;
    #define __DECLARED_NLINK_T
#endif

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
// Currently a pointer to uthread defined in pmos/tls.h
typedef void *pthread_t;
    #define __DECLARED_PTHREAD_T
#endif

#if (defined(__DECLARE_PTHREAD_MUTEX_T) || defined(__DECLARE_PTHREAD_COND_T)) && \
    !defined(__DECLARED___PTHREAD_WAITER)
struct __pthread_waiter {
    __pmos64u_t notification_port;
    struct __pthread_waiter *next;
};
    #define __DECLARED___PTHREAD_WAITER
#endif

#if defined(__DECLARE_PTHREAD_MUTEX_T) && !defined(__DECLARED_PTHREAD_MUTEX_T)
typedef struct {
    __pmos64u_t block_count;
    void * blocking_thread;
    struct __pthread_waiter *waiters_list_head;
    struct __pthread_waiter *waiters_list_tail;
    unsigned long recursive_lock_count;
    int type;
#ifdef __i386__
    int reserved;
} pthread_mutex_t __attribute__((aligned(8)));
#else
} pthread_mutex_t;
#endif
    #define __DECLARED_PTHREAD_MUTEX_T
#endif

#if defined(__DECLARE_PTHREAD_COND_T) && !defined(__DECLARED_PTHREAD_COND_T)
typedef struct {
    struct __pthread_waiter *waiters_list_head;
    struct __pthread_waiter *waiters_list_tail;
    int pop_spinlock;
} pthread_cond_t;
    #define __DECLARED_PTHREAD_COND_T
#endif

#if defined(__DECLARE_STACK_T) && !defined(__DECLARED_STACK_T)
typedef struct stack_t {
    void *ss_sp;
    unsigned long ss_size;
    int ss_flags;
} stack_t;
    #define __DECLARED_STACK_T
#endif

#if defined(__DECLARE_LOCALE_T) && !defined(__DECLARED_LOCALE_T)
typedef unsigned long locale_t;
    #define __DECLARED_LOCALE_T
#endif

#if defined(__DECLARE_SIGSET_T) && !defined(__DECLARED_SIGSET_T)
typedef unsigned long sigset_t;
    #define __DECLARED_SIGSET_T
#endif

#if defined(__DECLARE_TIMESPEC) && !defined(__DECLARED_TIMESPEC)
typedef struct timespec {
    time_t tv_sec;
    long tv_nsec;
};
    #define __DECLARED_TIMESPEC
#endif

#if defined(__DECLARE_FSBLKCNT_T) && !defined(__DECLARED_FSBLKCNT_T)
typedef __pmos64u_t fsblkcnt_t;
    #define __DECLARED_FSBLKCNT_T
#endif

#if defined(__DECLARE_FSFILCNT_T) && !defined(__DECLARED_FSFILCNT_T)
typedef __pmos64u_t fsfilcnt_t;
    #define __DECLARED_FSFILCNT_T
#endif

#ifndef _RESTRICT
    #ifdef __cplusplus
        #define _RESTRICT __restrict
    #else
        #define _RESTRICT restrict
    #endif
#endif

#ifndef _NORETURN
    // #ifdef __cplusplus
    //     #define _NORETURN [[noreturn]]
    // #else
    //     #define _NORETURN _Noreturn
    // #endif
    #define _NORETURN __attribute__((noreturn))
#endif