#include <sched/timers.hh>
#include <sbi/sbi.hh>

// Put a bogus value for now
u64 ticks_per_ms = 0;

// https://popovicu.com/posts/risc-v-interrupts-with-timer-example/
u64 get_current_timer_val()
{
    u64 value;
    asm volatile("rdtime %0" : "=r"(value));
    return value;
}

int fire_timer_at(u64 next_value)
{
    return sbi_set_timer(next_value).error;
}

void start_timer(u32 ms)
{
    const u64 ticks = ms * ticks_per_ms;
    const u64 current_value = get_current_timer_val();
    const u64 next_value = current_value + ticks;
    fire_timer_at(next_value);
}