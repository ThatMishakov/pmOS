#ifndef TIMERS_HEAP
#define TIMERS_HEAP
#include "timers.h"
#include <stdbool.h>

static const int timers_heap_min_size = 16;

void timer_push_heap(timer_entry entry);
bool timer_is_heap_empty();
timer_entry timer_get_front();
void timer_heap_pop();

#endif