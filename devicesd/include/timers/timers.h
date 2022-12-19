#ifndef TIMERS_H
#define TIMERS_G
#include <stdint.h>

extern enum Timer_Mode {
    TIMER_UNKNOWN,
    TIMER_HPET,
    TIMER_PIC
} timer_mode;

void init_timers();

uint64_t create_timer_ms(uint64_t ms);

#endif