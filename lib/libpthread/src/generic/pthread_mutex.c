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

#include <pmos/ports.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <assert.h>
#include <pthread.h>

#include "waiter_struct.h"

static __thread struct __pthread_waiter waiter_struct = {0, NULL};

int pthread_mutex_init(pthread_mutex_t* mutex, const pthread_mutexattr_t* attr) {
    // Check if attr is NULL and if not, its type
    if (attr != NULL && *attr != PTHREAD_MUTEX_NORMAL && *attr != PTHREAD_MUTEX_ERRORCHECK && *attr != PTHREAD_MUTEX_RECURSIVE) {
        errno = EINVAL;
        return -1;
    }

    mutex->blocking_thread_id = 0;
    mutex->waiters_list_head = NULL;
    mutex->recursive_lock_count = 0;
    mutex->type = *attr;
    mutex->block_count = 0;
    mutex->waiters_list_tail = NULL;
    return 0;
}

int sched_yield();

/// @brief Gets the thread ID of the current thread
/// @return The thread ID of the current thread
static uint64_t get_thread_id() {
    // Since getpid() returns the task ID instead of PID (which is tecnically wrong), use it for now
    return getpid();
}


// TODO: I've changed other parts of code without touching this, so it's very likely to be broken
static void atomic_pop_front(pthread_mutex_t* mutex) {
    assert(mutex->waiters_list_head != NULL);

    while (1) {
        // Atomically remove waiter from mutex->waiters_list_head
        struct __pthread_waiter* waiter = __atomic_load_n(&mutex->waiters_list_head, __ATOMIC_SEQ_CST);
        
        // Set the head to NULL
        __atomic_store_n(&mutex->waiters_list_head, NULL, __ATOMIC_SEQ_CST);

        // Check if the head is the only node in the list
        struct __pthread_waiter* waiters_tail = __atomic_load_n(&mutex->waiters_list_tail, __ATOMIC_SEQ_CST);
        if (waiters_tail == waiter) {
            // Try and set the tail to NULL

            if (__atomic_compare_exchange_n(&mutex->waiters_list_tail, &waiters_tail, NULL, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
                // Successfully removed waiter from the list
                return;
            }
        }

        // The waiter is not NULL, so we need to set the head to the next node
        struct __pthread_waiter* next = __atomic_load_n(&waiter->next, __ATOMIC_SEQ_CST);
        if (next == NULL) {
            // Wait for the next node to appear
            continue;
        }

        struct __pthread_waiter* null = NULL;

        // Set the head to the next node
        if (!__atomic_compare_exchange_n(&mutex->waiters_list_head, &null, next, 1, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
            // Failed to set the head to the next node
            sched_yield();
            continue;
        }

        // Successfully removed waiter from the list
        return;
    }
}

int pthread_mutex_lock(pthread_mutex_t* mutex) {
    uint64_t thread_id = get_thread_id();

    uint64_t blocking_thread_id = __atomic_load_n(&mutex->blocking_thread_id, __ATOMIC_SEQ_CST);
    if (blocking_thread_id == thread_id) {
        if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
            // Mutex has been acquired by the current thread
            mutex->recursive_lock_count++;
            return 0;
        }

        // Since we are here anyways, return the error no mater the type
        errno = EDEADLK;
        return -1;
    }

    // Atomically increment mutex->block_count
    uint64_t block_count = __atomic_fetch_add(&mutex->block_count, 1, __ATOMIC_SEQ_CST);

    if (block_count == 0) {
        // Mutex has been acquired
        mutex->blocking_thread_id = thread_id;
        return 0;
    }

    // Prepare waiter struct
    if (waiter_struct.notification_port == 0) {
        int result = __init_waiter_struct(&waiter_struct);
        if (result != 0) {
            // Could not prepare it. Atomically decrement mutex->block_count and return error
            __atomic_fetch_sub(&mutex->block_count, 1, __ATOMIC_SEQ_CST);
            return -1;
        }
    }

    // Add waiter to the lock-free list
    struct __pthread_waiter* waiter = &waiter_struct;

    __atomic_add_waiter(&mutex->waiters_list_head, &mutex->waiters_list_tail, waiter);

    // Wait for the mutex to be unlocked
    IPC_Mutex_Unlock unlock_signal;
    Message_Descriptor reply_descriptor;

    result_t result = syscall_get_message_info(&reply_descriptor, waiter->notification_port, 0);

    // This function should never fail unless something is really wrong
    assert(result == SUCCESS);

    atomic_pop_front(mutex);

    assert(reply_descriptor.size == sizeof(IPC_Mutex_Unlock));

    result = get_first_message((char *)&unlock_signal, sizeof(IPC_Mutex_Unlock), waiter->notification_port);

    // Again, this function should never fail unless something is really wrong
    assert(result == SUCCESS);

    // Mutex has been acquired
    mutex->blocking_thread_id = thread_id;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t* mutex)
{
    uint64_t thread_id = get_thread_id();

    uint64_t blocking_thread_id = __atomic_load_n(&mutex->blocking_thread_id, __ATOMIC_SEQ_CST);
    if (blocking_thread_id == thread_id) {
        if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
            // Mutex has been acquired by the current thread
            mutex->recursive_lock_count++;
            return 0;
        }

        // Since we are here anyways, return the error no mater the type
        errno = EDEADLK;
        return -1;
    }

    // Compare and set block count
    uint64_t block_count = 0;
    if (!__atomic_compare_exchange_n(&mutex->block_count, &block_count, 1, 0, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) {
        // Mutex is locked
        errno = EBUSY;
        return -1;
    }

    // Mutex has been acquired
    mutex->blocking_thread_id = thread_id;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t* mutex) {
    uint64_t thread_id = get_thread_id();

    // TODO: Other threads may potentially unlock it (?)
    if (mutex->blocking_thread_id != thread_id) {
        errno = EPERM;
        return -1;
    }

    if (mutex->recursive_lock_count > 0) {
        mutex->recursive_lock_count--;
        return 0;
    } else {
        mutex->blocking_thread_id = 0;
    }

    // Atomically decrement mutex->block_count
    uint64_t block_count = __atomic_fetch_sub(&mutex->block_count, 1, __ATOMIC_SEQ_CST);
    if (block_count == 0) {
        // No waiters
        return 0;
    }

    struct __pthread_waiter * head = __atomic_load_n(&mutex->waiters_list_head, __ATOMIC_SEQ_CST);

    // Wait for the tail to appear or its creation to fail
    while (block_count != 0 && head == NULL) {
        // Yield the CPU
        sched_yield();

        // Reload the head and block_count
        head = __atomic_load_n(&mutex->waiters_list_head, __ATOMIC_SEQ_CST);
        block_count = __atomic_load_n(&mutex->block_count, __ATOMIC_SEQ_CST);
    }

    // pthread_mutex_lock() failed
    if (block_count == 0) {
        return 0;
    }

    // Unlock the waiter
    IPC_Mutex_Unlock msg = {
        .type = IPC_Mutex_Unlock_NUM,
        .flags = 0,
    };

    result_t result = send_message_port(head->notification_port, sizeof(msg), (const char *)&msg);

    if (result != SUCCESS) {
        // Block the mutex
        __atomic_fetch_add(&mutex->block_count, 1, __ATOMIC_SEQ_CST);

        // Return an error
        errno = result;
        return -1;
    }

    // Mutex has been unlocked
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    *attr = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    if (type != PTHREAD_MUTEX_NORMAL && type != PTHREAD_MUTEX_ERRORCHECK && type != PTHREAD_MUTEX_RECURSIVE) {
        errno = EINVAL;
        return -1;
    }

    *attr = type;
    return 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type) {
    if (attr == NULL || type == NULL) {
        errno = EINVAL;
        return -1;
    }

    *type = *attr;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    if (attr == NULL) {
        errno = EINVAL;
        return -1;
    }

    *attr = 0;
    return 0;
}