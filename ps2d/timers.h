#ifndef TIMERS_H
#define TIMERS_H
#include <stdint.h>

static const unsigned timer_reply_chan = 12;
extern uint64_t tmr_index;

uint64_t start_timer(unsigned ms);

#endif