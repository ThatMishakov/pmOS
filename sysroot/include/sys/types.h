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
#define __DECLARE_INO_T
#define __DECLARE_NLINK_T

#include "../__posix_types.h"

typedef unsigned long fsblkcnt_t;
typedef unsigned long fsfilcnt_t;

typedef unsigned long key_t;

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