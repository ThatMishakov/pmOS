#include <timers/timers.h>
#include <timers/hpet.h>
#include <stddef.h>
#include <kernel/errors.h>
#include <stdlib.h>
#include <timers/timers_heap.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>


enum Timer_Mode timer_mode = TIMER_UNKNOWN;
uint64_t timer_ticks = 0;
uint64_t timer_id = 0;
timer_entry *current_timer = NULL;

void init_timers()
{
    init_hpet();

    if (hpet_is_functional)
        timer_mode = TIMER_HPET;
    
    // TODO: Fall back to PIC if HPET is not functional
    //if (hpet_virt == NULL)

    // Remove me: check timer
    program_periodic(100);
}

int start_timer(IPC_Start_Timer* t, uint64_t reply_pid)
{
    if (timer_mode == TIMER_UNKNOWN)
        return (int)ERROR_GENERAL;

    update_ticks();

    timer_entry *e = malloc(sizeof(timer_entry));
    e->id = timer_id++;
    e->extra0 = t->extra0;
    e->extra1 = t->extra1;
    e->extra2 = t->extra2;
    e->pid = reply_pid;
    e->reply_chan = t->reply_channel;

    if (t->ms == 0) {
        notify_task(e);
        free(e);
        return 0;
    }

    uint64_t ticks = timer_calculate_next_ticks(t->ms);
    e->expires_at_ticks = ticks + timer_ticks;

    timer_push_heap(e);

    // printf("Start timer %lx extra %li\n", e, e->extra);

    return 0;
}

void timer_tick()
{
    update_ticks();

    while (!timer_is_heap_empty()) {
        timer_entry* front = timer_get_front();
        if (front->expires_at_ticks > timer_ticks)
            break;

        timer_heap_pop();
        notify_task(front);
        free(front);
    }

    // printf("Tick! %lx\n", timer_ticks);
}

void notify_task(timer_entry* e)
{
    IPC_Timer_Reply r = {IPC_Timer_Reply_NUM, IPC_TIMER_TICK, e->id, e->extra0, e->extra1, e->extra2};

    send_message_port(e->reply_chan, sizeof(r), (char*)&r);
}

uint64_t timer_calculate_next_ticks(uint64_t ms)
{
    switch (timer_mode)
    {
    case TIMER_HPET:
        return hpet_calculate_ticks(ms);
        break;
    default:
        fprintf(stderr, "Warning: unknown timer mode %i\n", timer_mode);
        break;
    }
    return 0;
}

void update_ticks()
{
    switch (timer_mode)
    {
    case TIMER_HPET:
        hpet_update_system_ticks(&timer_ticks);
        break;
    
    default:
        break;
    }
}

void start_oneshot_ticks(uint64_t ticks)
{
    switch (timer_mode)
    {
        case TIMER_HPET:
            hpet_start_oneshot(ticks);
            break;
        default:
            break;
    }
}

void program_periodic(unsigned millis)
{
    switch (timer_mode)
    {
    case TIMER_HPET:
        hpet_program_periodic(hpet_calculate_ticks(millis));
        break;
    default:
        fprintf(stderr, "Warning: unknown timer mode %i\n", timer_mode);
        break;
    }
}