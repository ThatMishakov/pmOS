#pragma once
#include <types.hh>

// Ticks per millisecond for hart timers
extern u64 ticks_per_ms;

inline bool timer_needs_initialization()
{
    return ticks_per_ms == 0;
}

inline void set_timer_frequency(u64 frequency)
{
    ticks_per_ms = frequency/1000;
}

// Returns the current value of the timer
u64 get_current_timer_val();

