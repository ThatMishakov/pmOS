#ifndef TIMERS_H
#define TIMERS_H
#include <stdint.h>
#include <stdbool.h>

extern uint64_t ticks_ms;

extern enum Timer_Mode {
    TIMER_UNKNOWN,
    TIMER_HPET,
    TIMER_PIC
} timer_mode;

struct timer_info {
};

typedef struct timer_entry {
    uint64_t id;
    uint64_t extra;
    uint64_t expires_at_ticks;
} timer_entry;

void init_timers();

uint64_t create_timer_ms(uint64_t ms);
void timer_tick();


#endif