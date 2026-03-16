/* Copyright (c) 2024, Mikhail Kovalev
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "ports.h"

#include "commands.h"
#include "configuration.h"

#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum Port_States state = PORT_STATE_RESET;
enum Device_Types type = DEVICE_TYPE_UNKNOWN;
uint64_t last_timer;

uint32_t device_id = 0;
unsigned device_id_size = 0;

void (*managed_react_data)(unsigned char data) = NULL;
void (*managed_react_timer)() = NULL;

void start_port()
{
    state      = PORT_STATE_RESET;
    last_timer = 0;

    unsigned char data[] = {0xff};
    send_data_port(data, sizeof(data));
}

int send_data_port(const unsigned char *data, size_t data_size)
{
    size_t struct_size     = sizeof(IPC_PS2_Send_Data) + data_size;
    IPC_PS2_Send_Data *str = alloca(struct_size);

    str->type        = IPC_PS2_Send_Data_NUM;
    str->flags       = 0;
    memcpy(str->data, data, data_size);

    return (int)send_message_right(port_send_right, 0, (const char *)str, struct_size, NULL, 0).result;
}

int send_byte_port(unsigned char byte)
{
    return send_data_port(&byte, sizeof(byte));
}

void react_data(const char *data, size_t data_size)
{
    for (size_t i = 0; i < data_size; ++i)
        react_byte(data[i]);
}

void react_byte(unsigned char data)
{
    switch (state) {
    case PORT_STATE_RESET:
        // Device should respond ack and then SELF_TEST_OK or SELF_TEST_FAILURE
        // Ignore all other messages
        switch (data) {
        case RESPONSE_FAILURE:
            printf("[PS2d] Warning: Port reported failure during self-test...\n");
            break;

        case RESPONSE_SELF_TEST_OK:
            state = PORT_STATE_DISABLE_SCANNING;
            send_byte_port(COMMAND_DISABLE_SCANNING);
            port_start_timer(1000);
            break;

        case RESPONSE_ACK: // Ignore ACKs
            break;

        default: // Other data. Ignore it
            break;
        }
        break;

    case PORT_STATE_IDENTIFY:
        if (data == RESPONSE_ACK)
            // Ignore ACK
            break;

        if (data == RESPONSE_RESEND) {
            send_byte_port(COMMAND_IDENTIFY);
            break;
        }

        device_id |= data << (device_id_size * 8);
        device_id_size++;
        break;

    case PORT_STATE_WAIT:
        // Do nothing
        break;

    case PORT_STATE_DISABLE_SCANNING:
        switch (data) {
        case RESPONSE_ACK:
            // Disabled scanning. Start the identify secuence
            state = PORT_STATE_IDENTIFY;
            send_byte_port(COMMAND_IDENTIFY);
            port_start_timer(1000);
            break;
        case RESPONSE_RESEND:
            send_byte_port(COMMAND_DISABLE_SCANNING);
            break;

        default:
            // Ignore everything until scanning is disabled
            break;
        }
        break;

    case PORT_STATE_MANAGED:
        managed_react_data(data);
        break;
    }
}

void port_react_timer(uint64_t timer_id)
{
    if (last_timer != timer_id)
        return;

    switch (state) {
    case PORT_STATE_RESET:
        // Do nothing
        break;

    case PORT_STATE_DISABLE_SCANNING:
        fprintf(stderr,
                "[PS2d] Warning: Did not recieve the ACK for DISABLE_SCANNING command for port. Resetting the port...\n");
        state = PORT_STATE_WAIT;
        port_start_timer(5000);
        break;

    case PORT_STATE_WAIT:
        state = PORT_STATE_RESET;
        reset_port();
        break;

    case PORT_STATE_IDENTIFY:
        // Init port
        configure_port();
        break;

    case PORT_STATE_MANAGED:
        managed_react_timer();
        break;
    }
}

uint64_t tmr_index = 0;

uint64_t port_start_timer(unsigned time_ms)
{
    int result = pmos_request_timer(main_port, time_ms * 1000000, tmr_index);
    if (result != SUCCESS) {
        printf("[PS2d] Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    last_timer = tmr_index;

    // printf("Started timer index %i\n", tmr_index);

    return tmr_index++;
}

void react_timer(IPC_Timer_Reply *tmr)
{
    port_react_timer(tmr->timer_id);
}

void reset_port()
{
    state          = PORT_STATE_RESET;
    type           = DEVICE_TYPE_UNKNOWN;
    device_id_size = 0;
    send_byte_port(COMMAND_RESET);
}
