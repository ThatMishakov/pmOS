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

#include <errno.h>
#include <pmos/system.h>
#include <pthread.h>

int pthread_spin_trylock(pthread_spinlock_t *lock)
{
    if (__sync_lock_test_and_set(lock, 1) == 0) {
        // Acquired the lock
        return 0;
    } else {
        // Failed to acquire the lock
        return EBUSY;
    }
}

int sched_yield(void)
{
    result_t res = pmos_yield();
    if (res != SUCCESS) {
        errno = -res;
        return -1;
    }
    return 0;
}

int pthread_spin_lock(pthread_spinlock_t *lock)
{
    while (__sync_lock_test_and_set(lock, 1) != 0) {
        // Spin until the lock is acquired
        while (*lock) {
            // Yield the CPU to allow other threads to run
            sched_yield();
        }
    }
    return 0;
}

int pthread_spin_unlock(pthread_spinlock_t *lock)
{
    __sync_lock_release(lock);
    return 0;
}

int pthread_spin_init(pthread_spinlock_t *lock, int pshared)
{
    if (lock == NULL) {
        return EINVAL; // Invalid lock pointer
    }

    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED) {
        return EINVAL; // Invalid pshared value
    }

    *lock = 0; // Initialize the spinlock to an unlocked state

    return 0;
}

int pthread_spin_destroy(pthread_spinlock_t *lock)
{
    if (lock == NULL) {
        return EINVAL; // Invalid lock pointer
    }

    *lock = 0; // Reset the lock variable

    return 0;
}