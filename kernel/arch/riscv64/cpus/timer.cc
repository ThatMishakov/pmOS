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
#include <time/timers.hh>

using namespace kernel::sched;

// Put a bogus value for now
u64 ticks_per_ms = 0;

FreqFraction frequency_ns;
FreqFraction frequency_inv;

// https://popovicu.com/posts/risc-v-interrupts-with-timer-example/
u64 get_current_timer_val()
{
    u64 value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

int fire_timer_at(u64 next_value) { return sbi_set_timer(next_value).error; }

struct SbiTimer final: kernel::time::LocalTimer {
    virtual void set_deadline(u64 deadline_nanoseconds) override
    {
        u64 deadline_ticks = frequency_ns * deadline_nanoseconds;
        fire_timer_at(deadline_ticks);
    }

    virtual void cancel_deadline() override
    {
        fire_timer_at((u64)-1);
    }

    void init_as_main() override
    {
    }

    virtual const char *name() const override
    {
        return "SBI Timer";
    }
};
SbiTimer sbi_timer;

struct RiscvTimer final: kernel::time::TimeSource {
    virtual u64 get_absolute_time() const override
    {
        return frequency_inv * get_current_timer_val();
    }

    virtual const char *name() const override
    {
        return "RISC-V Timer";
    }

    virtual void init_as_main() override
    {
    }
};
RiscvTimer riscv_timer;

void set_timer_frequency(u64 frequency)
{
    frequency_ns = computeFreqFraction(frequency, 1'000'000'000);
    frequency_inv = computeFreqFraction(1'000'000'000, frequency);
    ticks_per_ms = frequency / 1000;

    kernel::time::kernel_timesource = &riscv_timer;
    kernel::time::kernel_local_timer = &sbi_timer;
}