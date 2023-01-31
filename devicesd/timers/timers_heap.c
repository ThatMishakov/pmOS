#include <timers/timers_heap.h>
#include <stdlib.h>
#include <stdio.h>
#include <timers/timers.h>

unsigned long timers_heap_size = 0;
unsigned long timers_heap_capacity = 0;
timer_entry **timers_heap_array = NULL;

void timers_heap_grow()
{
    unsigned long new_capacity = 0;
    if (timers_heap_capacity < timers_heap_min_size) {
        new_capacity = timers_heap_min_size;
    } else {
        new_capacity = timers_heap_capacity*2;
    }

    timers_heap_array = realloc(timers_heap_array, new_capacity*sizeof(timer_entry));
    timers_heap_capacity = new_capacity;
}

bool timer_is_heap_empty()
{
    return timers_heap_size == 0;
}

timer_entry* timer_get_front()
{
    return timers_heap_array[0];
}

static inline unsigned long left(unsigned long i)
{
    return i*2 + 1;
}

static inline unsigned long right(unsigned long i)
{
    return i*2 + 2;
}

static inline unsigned long parent(unsigned long i)
{
    return (i-1)/2;
}

void swap(timer_entry **a, timer_entry **b)
{
    timer_entry *tmp = *a;
    *a = *b;
    *b = tmp;
}

void timer_push_heap(timer_entry *entry)
{
    if (timers_heap_size == timers_heap_capacity)
        timers_heap_grow();

    unsigned long i = timers_heap_size;
    ++timers_heap_size;

    unsigned long p = parent(i);
    while (i != 0 && timers_heap_array[p]->expires_at_ticks > entry->expires_at_ticks) {
        timers_heap_array[i] = timers_heap_array[p];
        i = p;
        p = parent(i);
    }

    timers_heap_array[i] = entry;
}

void heapify_up(unsigned long i)
{
    unsigned long min = i;

    unsigned long l = left(i);
    if (l < timers_heap_size && timers_heap_array[i]->expires_at_ticks > timers_heap_array[l]->expires_at_ticks)
        min = l;

    unsigned long r = right(i);
    if (r < timers_heap_size && timers_heap_array[i]->expires_at_ticks > timers_heap_array[r]->expires_at_ticks)
        min = r;

    if (min != i) {
        timer_entry *temp = timers_heap_array[i];
        timers_heap_array[i] = timers_heap_array[min];
        timers_heap_array[min] = temp;

        heapify_up(min);
    }
}

void timer_heap_pop()
{
    timers_heap_array[0] = timers_heap_array[timers_heap_size - 1];
    timers_heap_size--;

    heapify_up(0);

    if ((timers_heap_capacity > timers_heap_min_size) && (timers_heap_size == timers_heap_capacity/2)) {
        timers_heap_capacity = timers_heap_size;
        timers_heap_array = realloc(timers_heap_array, sizeof(timer_entry)*timers_heap_capacity);
    }
}

