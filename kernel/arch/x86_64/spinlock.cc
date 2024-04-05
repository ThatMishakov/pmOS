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

#include "types.hh"
#include "utils.hh"
#include <kern_logger/kern_logger.hh>

int lock_wanted = true;
int lock_free = false;

void Spinlock::lock() noexcept
{
    int counter = 0;
    bool result;
    do {
        result = __atomic_compare_exchange(&locked, &lock_free, &lock_wanted, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
        if (counter++ == 10000000) {
            t_print_bochs("Possible deadlock at spinlock 0x%h... ", this);
            print_stack_trace(bochs_logger);
        }
    } while (not result);
}

void Spinlock::unlock() noexcept
{
	__atomic_store_n(&locked, false, __ATOMIC_RELEASE);
}

bool Spinlock::try_lock() noexcept
{
    return __atomic_compare_exchange(&locked, &lock_free, &lock_wanted, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
}

extern "C" void acquire_lock_spin(int *var)
{
    int counter = 0;
    bool result;
    do {
        result = __atomic_compare_exchange(var, &lock_free, &lock_wanted, true, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED);
        if (counter++ == 10000000) {
            t_print_bochs("Possible deadlock at spinlock 0x%h... ", var);
            print_stack_trace(bochs_logger);
        }
    } while (not result);
}

extern "C" void release_lock(int *var)
{
    __atomic_store_n(var, false, __ATOMIC_RELEASE);
}