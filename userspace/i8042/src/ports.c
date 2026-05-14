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

#include "io.h"
#include "registers.h"

#include <alloca.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pmos/helpers.h>
#include <inttypes.h>

Port ports[2];

bool send_data_port(uint8_t cmd, bool port_2)
{
    unsigned count = 0;
    uint8_t status = 0;

    if (port_2) {
        write_wait(RW_PORT, SECOND_PORT);
    }
    write_wait(DATA_PORT, cmd);
    return true;
}

bool read_data(char *data, bool *is_second)
{
    if (!data)
        return false;

    uint8_t status = inb(RW_PORT);
    if (!(status & STATUS_MASK_OUTPUT_FULL)) {
        // No data to read
        return false;
    }

    if (is_second) {
        *is_second = !!(status & STATUS_MASK_SECOND_FULL);
    }

    *data = inb(DATA_PORT);

    return true;
}

void react_data(uint8_t data, unsigned char port_num)
{
    if (!ports[port_num].notification_right) {
        printf("[i8042] Discarding data (0x%X) for port %i with no notification right...\n", data, port_num);
        return;
    }

    size_t size = sizeof(IPC_PS2_Notify_Data) + 1;

    IPC_PS2_Notify_Data *str = alloca(size);

    str->type          = IPC_PS2_Notify_Data_NUM;
    str->flags         = 0;
    str->data[0]       = data;

    result_t result = send_message_right(ports[port_num].notification_right, 0, (char *)str, size, NULL, 0).result;
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to react to data\n");
    }
}

void react_port_int(unsigned port_num)
{
    char data   = 0;
    bool second = port_num;

    bool two_ports_work = first_port_works && second_port_works;

    bool *check_second  = two_ports_work ? &second : NULL;
    bool have_read_data = read_data(&data, check_second);

    if (!have_read_data)
        return;

    react_data(data, second);
}

void react_port1_int() { react_port_int(0); }

void react_port2_int() { react_port_int(1); }

int data_callback(Message_Descriptor *desc, void *message,
                                       pmos_right_t *, pmos_right_t *,
                                       void *ctx, struct pmos_msgloop_data *)
{
    Port *p = ctx;
    if (desc->size < sizeof(IPC_PS2_Send_Data)) {
        fprintf(stderr, "[i8042] Warning: IPC_PS2_Send_Data with size %i smaller than expected\n",
                (int)desc->size);
        return PMOS_MSGLOOP_CONTINUE;
    }

    IPC_PS2_Send_Data *str = message;
    if (str->type != IPC_PS2_Send_Data_NUM) {
        fprintf(stderr, "[i8042] Warning: Unknown message type %" PRIu32 " in data callback. Expected IPC_PS2_Send_Data_NUM\n", str->type);
        return PMOS_MSGLOOP_CONTINUE;
    }

    unsigned port    = p != ports;
    size_t data_size = desc->size - sizeof(IPC_PS2_Send_Data);

    int mutex_result = pthread_mutex_lock(&ports_mutex);
    if (mutex_result != 0) {
        fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
        return PMOS_MSGLOOP_CONTINUE;
    }

    for (size_t i = 0; i < data_size; ++i)
        send_data_port(str->data[i], port);

    mutex_result = pthread_mutex_unlock(&ports_mutex);
    if (mutex_result != 0) {
        fprintf(stderr, "[i8042] Error: Could not unlock mutex\n");
        exit(mutex_result);
    }

    return PMOS_MSGLOOP_CONTINUE;
}

extern pmos_port_t main_port;
extern struct pmos_msgloop_data msgloop_data;

void disable_port(unsigned port)
{
    if (port == 0) {
        write_wait(RW_PORT, DISABLE_FIRST_PORT);
    } else {
        write_wait(RW_PORT, DISABLE_SECOND_PORT);
    }
}

void enable_port(unsigned port)
{
    if (port == 0) {
        write_wait(RW_PORT, ENABLE_FIRST_PORT);
    } else {
        write_wait(RW_PORT, ENABLE_SECOND_PORT);
    }
}

static void port_remove_recieve_right(Port *p)
{
    if (p->port_data_recieve_right) {
        delete_receive_right(main_port, p->port_data_recieve_right);
        pmos_msgloop_erase(&msgloop_data, &p->port_data_node);
        p->port_data_recieve_right = 0;
        disable_port(p != ports);
    }
}
static pmos_right_t arrange_data_right(Port *p)
{
    port_remove_recieve_right(p);
    
    pmos_right_t rr;
    right_request_t req = create_right(main_port, &rr, 0);
    if (req.result) {
        fprintf(stderr, "[i8042] Failed to create the right for a port: %i (%s)\n", (int)req.result, strerror(-(int)req.result));
        exit(1);
    }

    p->port_data_recieve_right = rr;

    pmos_msgloop_node_set(&p->port_data_node, rr, data_callback, p);
    pmos_msgloop_insert(&msgloop_data, &p->port_data_node);

    return req.right;

}

void react_register_port(unsigned port, IPC_PS2_Reg_Port *msg, pmos_right_t *reply_right, pmos_right_t *other_rights)
{
    printf("[i8042] Registering port...\n");
    (void)msg;

    if (!*reply_right) {
        fprintf(stderr, "[i8042] Warning: Recieved register port request for port %u with no reply right...\n", port);
        return;
    }

    if (!other_rights[0]) {
        fprintf(stderr, "[i8042] Warning: Recieved register port request for port %u with no send right...\n", port);
        return;
    }

    Port *p = ports + port;
    pmos_right_t right = arrange_data_right(p);

    IPC_PS2_Reg_Port_Reply reply = {
        .type = IPC_PS2_Reg_Port_Reply_NUM,
        .flags = 0,
        .result = 0,
    };

    message_extra_t e = {
        .extra_rights = {right, },
    };

    right_request_t req = send_message_right(*reply_right, 0, &reply, sizeof(reply), &e, SEND_MESSAGE_DELETE_RIGHT);
    if (req.result) {
        fprintf(stderr, "[i8042] Error: Failed to send reply to right register request: %i (%s)\n", (int)req.result, strerror(-(int)req.result));
        port_remove_recieve_right(p);
        return;
    }
    p->notification_right = other_rights[0];

    enable_port(port);

    *reply_right = 0;
    other_rights[0] = 0;
}