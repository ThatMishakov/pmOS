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

// Credits/inspiration:
// https://www.kernel.org/doc/ols/2001/read-copy.pdf
// https://github.com/Keyronex/Keyronex/blob/master/kernel/nanokern/rcu.c

#pragma once
#include <assert.h>
#include <lib/vector.hh>
#include <types.hh>

/**
 * @brief RCU function type
 *
 * This function gets called when the RCU is done with the current generation
 *
 * @param self Pointer to the object that the RCU is done with
 * @param chained True if the next RCU callback has the same function pointer (for example, when
 * releasing a list of pages)
 */
typedef void (*rcu_func_t)(void *self, bool chained);
struct RCU_Head {
    RCU_Head *rcu_next;
    rcu_func_t rcu_func;
};

extern size_t number_of_cpus;

class RCU
{
public:
    inline RCU() { assert(bitmask.resize((number_of_cpus + 63) / 64, 0)); }

    ~RCU() = default;

private:
    Spinlock lock;
    klib::vector<u64> bitmask;

    u64 generation         = 0;
    u64 highest_generation = 0;

    bool cpu_bit_set(size_t cpu_id) noexcept;
    void silence_cpu(size_t cpu_id) noexcept;
    bool generation_complete() noexcept;
    void start_generation() noexcept;

    friend struct RCU_CPU;
};

struct RCU_CPU {
    RCU_Head *current_callbacks = nullptr, *next_callbacks = nullptr;
    u64 generation = 0;

    void push(RCU_Head *head)
    {
        head->rcu_next = next_callbacks;
        next_callbacks = head;
    }

    void quiet(RCU &parent, size_t my_cpu_id);
};