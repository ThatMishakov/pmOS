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

#include <sbi/sbi.hh>
#include <sched/sched.hh>
#include <sched/timers.hh>

using namespace kernel::sched;

// Put a bogus value for now
u64 ticks_per_ms = 0;

FreqFraction frequency_ns;
FreqFraction frequency_inv;

void set_timer_frequency(u64 frequency)
{
    frequency_ns = computeFreqFraction(frequency, 1e9);
    frequency_inv = computeFreqFraction(1e9, frequency);
    ticks_per_ms = frequency / 1000;
}

// https://popovicu.com/posts/risc-v-interrupts-with-timer-example/
u64 get_current_timer_val()
{
    u64 value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

int fire_timer_at(u64 next_value) { return sbi_set_timer(next_value).error; }

#include <kern_logger/kern_logger.hh>

u64 kernel::sched::ticks_since_bootup = 0;

u64 kernel::sched::get_ns_since_bootup() { 
    return frequency_inv * get_current_timer_val();
}

void kernel::sched::maybe_rearm_timer(u64 deadline_nanoseconds)
{
    auto c = get_cpu_struct();
    if (c->local_timer_next_deadline != 0 and (c->local_timer_next_deadline < deadline_nanoseconds))
        return;

    c->local_timer_next_deadline = deadline_nanoseconds;
    u64 ticks = frequency_ns * deadline_nanoseconds;
    fire_timer_at(ticks);
}