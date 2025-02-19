/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
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
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <kern_logger/kern_logger.hh>
#include <types.hh>
#include <utils.hh>

void acquire_lock_spin(u32 *lock) noexcept
{
    //size_t count = 0;
    while (true) {
        while (__atomic_load_n(lock, __ATOMIC_RELAXED)) {
            // if (count++ > 1000000) {
            //     count = 0;
            //     serial_logger.printf("Warning: spinlock is busy\n");
            //     print_stack_trace(serial_logger);
            // }
        }

        if (!__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
            return;
    }
}
void release_lock(u32 *lock) noexcept { __atomic_store_n(lock, 0, __ATOMIC_RELEASE); }

constexpr int max_spins = 32;
bool try_lock(u32 *lock) noexcept { 
    for (int i = 0; i < max_spins; i++) {
        if (__atomic_load_n(lock, __ATOMIC_RELAXED))
            continue;

        if (__atomic_exchange_n(lock, 1, __ATOMIC_ACQUIRE))
            continue;

        return true;
    }
    return false;
}

void Spinlock_base::lock() noexcept { 
    acquire_lock_spin(&locked);
}

void Spinlock_base::unlock() noexcept { release_lock(&locked); }

bool Spinlock_base::try_lock() noexcept { return ::try_lock(&locked); }