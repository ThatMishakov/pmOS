#include <timers/timers.h>
#include <timers/hpet.h>
#include <stddef.h>

enum Timer_Mode timer_mode = TIMER_UNKNOWN;

void init_timers()
{
    init_hpet();

    if (hpet_virt != NULL)
        timer_mode = TIMER_HPET;
    
    // TODO: Fall back to PIC if HPET is not functional
    //if (hpet_virt == NULL)
}

void timer_tick()
{
    printf("Tic-toc\n");
}