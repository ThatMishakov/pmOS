#include "timers.h"
#include <pmos/system.h>
#include <kernel/block.h>
#include <pmos/helpers.h>
#include "ports.h"
#include <pmos/ipc.h>
#include <stdint.h>
#include <stdio.h>
#include <pmos/ports.h>

uint64_t tmr_index = 1;
extern pmos_port_t configuration_port;
extern pmos_port_t main_port;
extern pmos_port_t devicesd_port;


uint64_t start_timer(unsigned ms)
{
    IPC_Start_Timer tmr = {IPC_Start_Timer_NUM, ms, main_port, tmr_index, 0, 0};

    result_t result = send_message_port(devicesd_port, sizeof(tmr), (char*)&tmr);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    return tmr_index++;
}