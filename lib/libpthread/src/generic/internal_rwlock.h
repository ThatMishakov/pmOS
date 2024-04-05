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

#ifndef INTERNAL_RWLOCK_H
#define INTERNAL_RWLOCK_H

// An internal implementation of a read-write lock. Although this provides
// the same functionality as the pthread_rwlock_t type (and worse performance),
// it is using spinlocks which don't make syscalls and thus can never fail, which
// is wanted for stuff like pthread_key_create which must call destructors
// on thread exit, needing to lock key structure, where failure cannot be handled.
//
// I believe this is a good tradeoff, since I think it doesn't make much difference
// how quickly threads can terminate, and it's better to have something simple with
// a guarantee that destructors could be called even when everything is on fire.

#include <stdint.h>
#include <stdbool.h>

// https://en.wikipedia.org/wiki/Readers%E2%80%93writer_lock#Using_two_mutexes
struct internal_rwlock {
    uint32_t r;
    uint32_t g;
    uint64_t readers_count;
};

#define __INTERNAL_RWLOCK_INITIALIZER { 0, 0, 0 }

void __internal_rwlock_spinlock(uint32_t *lock);
void __internal_rwlock_spinunlock(uint32_t *lock);

void __internal_rwlock_read_lock(struct internal_rwlock *lock);
void __internal_rwlock_read_unlock(struct internal_rwlock *lock);
void __internal_rwlock_write_lock(struct internal_rwlock *lock);
void __internal_rwlock_write_unlock(struct internal_rwlock *lock);

#endif // INTERNAL_RWLOCK_H