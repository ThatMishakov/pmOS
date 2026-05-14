#include "timer.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>

extern pmos_port_t main_port;
extern struct pmos_msgloop_data msgloop_data;

static pmos_msgloop_tree_node_t timer_node;
static pmos_right_t timer_recieve_right = INVALID_RIGHT;

uint64_t next_timer_deadline = 0;

static int timer_callback(Message_Descriptor *desc, void *buff, pmos_right_t *,
                     pmos_right_t *, void *, struct pmos_msgloop_data *)
{
    assert(desc->size >= sizeof(IPC_Timer_Expired));
    IPC_Timer_Expired *timer_msg = (IPC_Timer_Expired *)buff;
    assert(timer_msg->type == IPC_Timer_Expired_NUM);

    uint64_t deadline = timer_msg->deadline;
    if (deadline >= next_timer_deadline)
        port_react_timer();

    return PMOS_MSGLOOP_CONTINUE;
}

void init_timer()
{
    right_request_t req = pmos_create_timer(main_port);
    if (req.result) {
        fprintf(stderr, "[PS2d] Failed to create timer right: %i (%s)\n", (int)req.result, strerror(-(int)req.result));
        exit(1);
    }
    timer_recieve_right = req.right;

    pmos_msgloop_node_set(&timer_node, timer_recieve_right, timer_callback, &msgloop_data);
    pmos_msgloop_insert(&msgloop_data, &timer_node);
}

void port_start_timer(unsigned time_ms)
{
    syscall_r current_time = pmos_get_time(GET_TIME_NANOSECONDS_SINCE_BOOTUP);
    if (current_time.result) {
        fprintf(stderr, "[PS2d] Failed to get time: %i (%s)\n", (int)current_time.result, strerror(-(int)current_time.result));
        exit(1);
    }

    next_timer_deadline = current_time.value + (uint64_t)time_ms * 1'000'000;
    result_t result = pmos_set_timer(main_port, timer_recieve_right, next_timer_deadline, 0);
    if (result) {
        fprintf(stderr, "[PS2d] Failed to set timer: %i (%s)\n", (int)result, strerror(-result));
        exit(1);
    }
}