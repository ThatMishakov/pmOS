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

#include "waiter_struct.h"

#include <assert.h>
#include <errno.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <pthread.h>

__thread struct __pthread_waiter cond_waiter_struct = {0, NULL};

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr)
{
    // Check if attr is NULL and if not, its type
    // TODO: Condattr are not implemented yet
    // if (attr != NULL) {
    //     errno = EINVAL;
    //     return -1;
    // }

    cond->waiters_list_head = NULL;
    cond->waiters_list_tail = NULL;
    return 0;
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *)
{
    // Sorry not sorry
    return pthread_cond_wait(cond, mutex);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)
{
    // Check if mutex is NULL
    if (mutex == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check if mutex is locked by the calling thread
    // TODO
    // if (mutex->blocking_thread_id != pthread_self()) {
    //     errno = EPERM;
    //     return -1;
    // }

    // Prepare waiter struct
    if (cond_waiter_struct.notification_port == INVALID_PORT) {
        if (__init_waiter_struct(&cond_waiter_struct) != 0) {
            return -1;
        }
    }

    // Add waiter to the list. Do this before unlocking mutex to have a predictable order of waiters
    __atomic_add_waiter(&cond->waiters_list_head, &cond->waiters_list_tail, &cond_waiter_struct);

    // Unlock the mutex
    pthread_mutex_unlock(mutex);

    // Wait for an unlock message
    IPC_Mutex_Unlock unlock_signal;
    Message_Descriptor reply_descriptor;

    result_t result =
        syscall_get_message_info(&reply_descriptor, cond_waiter_struct.notification_port, 0);

    // This should never fail
    assert(result == SUCCESS);

    // Check if the message is the unlock signal
    assert(reply_descriptor.size >= sizeof(IPC_Mutex_Unlock));

    result = get_first_message((char *)&unlock_signal, MSG_ARG_REJECT_RIGHT,
                               cond_waiter_struct.notification_port).result;

    // Again, this should never fail
    assert(result == SUCCESS);

    assert(unlock_signal.type == IPC_Mutex_Unlock_NUM);

    // We've been woken up. Lock the mutex again
    int lock_result = pthread_mutex_lock(mutex);

    // In theory, mutex can only fail the first time it is locked, so if it does something screwy is
    // going on
    assert(lock_result == 0);

    return 0;
}

static void simple_lock(int *lock)
{
    static int simple_lock_expected = 0;

    do {
        if (*lock == 1) {
            sched_yield();
            continue;
        }

        if (__atomic_compare_exchange_n(lock, &simple_lock_expected, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            return;
        }
    } while (1);
}

static void simple_unlock(int *lock) { __atomic_store_n(lock, 0, __ATOMIC_SEQ_CST); }

int pthread_cond_broadcast(pthread_cond_t *cond)
{
    // Check if cond is NULL
    if (cond == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check if cond has any waiters
    if (cond->waiters_list_head == NULL) {
        return 0;
    }

    // Send unlock messages to all waiters
    IPC_Mutex_Unlock unlock_signal = {
        .type = IPC_Mutex_Unlock_NUM,
        .flags = 0,
    };

    // Only one thread can call pop at the time, so protect it with a spinlock.
    // Originally, I was planning to use N producers/N consumers lock-free queue, but
    // that turned out to be very complicated and since most of the time only
    // one thread is expected to broadcast, protecting pop with a spinlock
    // should be fine and also allows for a simplier (and potentially faster)
    // implementation of enqueue

    simple_lock(&cond->pop_spinlock);
    struct __pthread_waiter *waiter =
        __try_pop_waiter(&cond->waiters_list_head, &cond->waiters_list_tail);
    simple_unlock(&cond->pop_spinlock);

    int fail_count = 0;

    while (waiter != NULL) {
        result_t result = send_message_port(waiter->notification_port, sizeof(IPC_Mutex_Unlock),
                                            (char *)&unlock_signal);

        if (result != SUCCESS) {
            fail_count++;
            __atomic_add_waiter(&cond->waiters_list_head, &cond->waiters_list_tail, waiter);

            if (fail_count > 10) {
                errno = EAGAIN;
                return -1;
            }
        }

        simple_lock(&cond->pop_spinlock);
        struct __pthread_waiter *waiter =
            __try_pop_waiter(&cond->waiters_list_head, &cond->waiters_list_tail);
        simple_unlock(&cond->pop_spinlock);
        assert(waiter != NULL);
    }

    return 0;
}

int pthread_cond_signal(pthread_cond_t *cond)
{
    // Check if cond is NULL
    if (cond == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check if cond has any waiters
    if (cond->waiters_list_head == NULL) {
        return 0;
    }

    // Send unlock message to the first waiter
    IPC_Mutex_Unlock unlock_signal = {
        .type = IPC_Mutex_Unlock_NUM,
        .flags = 0
    };

    simple_lock(&cond->pop_spinlock);
    struct __pthread_waiter *waiter =
        __try_pop_waiter(&cond->waiters_list_head, &cond->waiters_list_tail);
    simple_unlock(&cond->pop_spinlock);

    if (waiter == NULL) {
        return 0;
    }

    result_t result = send_message_port(waiter->notification_port, sizeof(IPC_Mutex_Unlock),
                                        (char *)&unlock_signal);

    if (result != SUCCESS) {
        __atomic_add_waiter(&cond->waiters_list_head, &cond->waiters_list_tail, waiter);
        errno = EAGAIN;
        return -1;
    }

    return 0;
}


int pthread_cond_destroy(pthread_cond_t *cond)
{
    // Check if cond is NULL
    if (cond == NULL) {
        errno = EINVAL;
        return -1;
    }

    // Check if cond has any waiters
    if (cond->waiters_list_head != NULL) {
        errno = EBUSY;
        return -1;
    }

    return 0;
}

int pthread_condattr_init(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    *attr = 0;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr)
{
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *attr, clockid_t clock_id)
{
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    switch (clock_id) {
        case CLOCK_REALTIME:
            *attr |= 0x01;
            return 0;
        case CLOCK_MONOTONIC:
            *attr &= ~0x01;
            return 0;
        default:
            errno = EINVAL;
            return -1;
    }

    return 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *attr, clockid_t *clock_id)
{
    if (attr == NULL || clock_id == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (*attr & 0x01) {
        *clock_id = CLOCK_REALTIME;
    } else {
        *clock_id = CLOCK_MONOTONIC;
    }

    return 0;
}