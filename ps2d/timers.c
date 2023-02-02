#include "timers.h"
#include <pmos/system.h>
#include <kernel/block.h>
#include <pmos/helpers.h>
#include "ports.h"
#include <pmos/ipc.h>
#include <stdint.h>
#include <stdio.h>

uint64_t tmr_index = 1;

uint64_t start_timer(unsigned ms)
{
    IPC_Start_Timer_Str tmr = {IPC_Start_Timer_NUM, ms, tmr_index, timer_reply_chan};

    result_t result = send_message_port(1024, sizeof(tmr), (char*)&tmr);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    //printf("Started timer index %i\n", tmr_index);

    return tmr_index++;
}