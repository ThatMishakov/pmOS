#include "timers.hh"
#include <interrupts/apic.hh>
#include "sched/sched.hh"

void start_timer_ticks(u32 ticks)
{
    apic_one_shot_ticks(ticks);
    get_cpu_struct()->timer_val = ticks;
}

void start_timer(u32 ms)
{
    u32 ticks = ticks_per_1_ms*ms;
    start_timer_ticks(ticks);
}