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

#include "internal_rwlock.h"
#include "spin_pause.h"

void __internal_rwlock_read_lock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->r);
    lock->readers_count++;
    if (lock->readers_count == 1)
        __internal_rwlock_spinlock(&lock->g);
    __internal_rwlock_spinunlock(&lock->r);
}

void __internal_rwlock_read_unlock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->r);
    lock->readers_count--;
    if (lock->readers_count == 0)
        __internal_rwlock_spinunlock(&lock->g);
    __internal_rwlock_spinunlock(&lock->r);
}

void __internal_rwlock_write_lock(struct internal_rwlock *lock) {
    __internal_rwlock_spinlock(&lock->g);
}

void __internal_rwlock_write_unlock(struct internal_rwlock *lock) {
    __internal_rwlock_spinunlock(&lock->g);
}



void __internal_rwlock_spinunlock(uint32_t *lock) {
    __atomic_store_n(lock, 0, __ATOMIC_RELEASE);
}

void __internal_rwlock_spinlock(uint32_t *lock) {
    uint32_t expected = 0;
    while (1) {
        if (__atomic_compare_exchange_n(lock, &expected, 1, 0, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return;

        while (__atomic_load_n(lock, __ATOMIC_RELAXED))
            __spin_pause();
    }
}