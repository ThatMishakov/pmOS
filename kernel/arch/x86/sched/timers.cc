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

#include <interrupts/apic.hh>
#include <sched/sched.hh>
#include <x86_asm.hh>

using namespace kernel::sched;
using namespace kernel;
using namespace kernel::x86::interrupts::lapic;

u64 kernel::sched::ticks_since_bootup = 0;
void start_timer_ticks(u32 ticks)
{
    auto t = apic_get_remaining_ticks();
    apic_one_shot_ticks(ticks);
    auto c = get_cpu_struct();
    c->system_timer_val += c->timer_val - t;
    c->timer_val = ticks;

    if (c->is_bootstap_cpu())
        ticks_since_bootup = c->system_timer_val;
}

void start_timer(u32 ms)
{
    u64 ticks = apic_freq * (ms * 1'000'000);
    start_timer_ticks(ticks);
}

u64 get_current_time_ticks()
{
    auto c = get_cpu_struct();
    return c->system_timer_val + c->timer_val - apic_get_remaining_ticks();
}

extern bool have_invariant_tsc;
extern u64 boot_tsc;

u64 kernel::sched::get_ns_since_bootup() { 
    if (have_invariant_tsc) {
        u64 tsc = rdtsc() - boot_tsc;
        return tsc_inverted_freq * tsc;
    }
    return apic_inverted_freq * ticks_since_bootup;
}

u64 CPU_Info::ticks_after_ms(u64 ms) { return ticks_after_ns(ms * 1'000'000); }
u64 CPU_Info::ticks_after_ns(u64 ns)
{
    return get_current_time_ticks() + (apic_freq * ns);
}