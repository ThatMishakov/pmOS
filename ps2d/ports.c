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
#include <pmos/system.h>
#include <stdlib.h>
#include <stdio.h>
#include <alloca.h>
#include <string.h>
#include <pmos/ipc.h>
#include "commands.h"
#include "configuration.h"

struct port_list_node *first = NULL;
struct port_list_node *last = NULL;

void start_port(struct port_list_node *port)
{
    port->state = PORT_STATE_RESET;
    port->last_timer = 0;

    unsigned char data[] = {0xff};
    send_data_port(port, data, sizeof(data));
}

extern pmos_port_t main_port;
extern pmos_port_t devicesd_port;
bool register_port(IPC_PS2_Reg_Port* message, uint64_t sender)
{
    uint64_t port_id = message->internal_id;

    struct port_list_node* ptr = list_get(sender, port_id);

    if (ptr != NULL) {
        printf("Port already exists\n");
        IPC_PS2_Config reply = {
            .type = IPC_PS2_Config_NUM,
            .flags = 0,
            .internal_id = port_id,
            .request_type = IPC_PS2_Config_Reg_Port,
            .result_cmd = 0,
        };

        send_message_port(message->config_port, sizeof(reply), (char *)&reply);

        return false;
    }

    IPC_PS2_Config reply = {
        .type = IPC_PS2_Config_NUM,
        .flags = 0,
        .internal_id = port_id,
        .request_type = IPC_PS2_Config_Reg_Port,
        .result_cmd = main_port,
    };

    result_t result;
    if ((result = send_message_port(message->config_port, sizeof(reply), (char *)&reply)) != SUCCESS) {
        printf("[PS2d] Warning: Could not reply to config request to task %li port %li\n", sender, message->config_port);
        return false;
    }

    ptr = malloc(sizeof (struct port_list_node));

    if (ptr == NULL)
        return false;

    ptr->com_port = message->cmd_port;
    ptr->last_timer = 0;
    ptr->owner_pid = sender;
    ptr->state = 0;
    ptr->port_id = port_id;
    ptr->state = PORT_STATE_RESET;
    ptr->type = DEVICE_TYPE_UNKNOWN;
    ptr->device_id_size = 0;
    ptr->next = NULL;
    ptr->previous = NULL;

    list_push_back(ptr);

    start_port(ptr);

    return true;
}

void send_data_port(struct port_list_node *port, const unsigned char *data, size_t data_size)
{
    size_t struct_size = sizeof(IPC_PS2_Send_Data) + data_size;
    IPC_PS2_Send_Data *str = alloca(struct_size);

    str->type = IPC_PS2_Send_Data_NUM;
    str->flags = 0;
    str->internal_id = port->port_id;
    memcpy(str->data, data, data_size);

    send_message_port(port->com_port, struct_size, (const char *)str);
}

void send_byte_port(struct port_list_node *port, unsigned char byte)
{
    send_data_port(port, &byte, sizeof(byte));
}

struct port_list_node *list_get(uint64_t owner_pid, uint64_t port_id)
{
    struct port_list_node *ptr = first;
    while (ptr != NULL && (ptr->owner_pid != owner_pid || ptr->port_id != port_id))
        ptr = ptr->next;

    return ptr;
}

void list_push_back(struct port_list_node *n)
{
    if (last != NULL) {
        last->next = n;
        n->previous = last;
        n->next = NULL;
        last = n;
    } else {
        n->previous = NULL;
        n->next = NULL;
        first = n;
        last = n;
    }
}

void react_message(IPC_PS2_Notify_Data* message, uint64_t sender, uint64_t message_size)
{
    struct port_list_node *ptr = list_get(sender, message->internal_id);

    if (ptr == NULL) {
        fprintf(stderr, "[PS2d] Warning: Recieved message from unregistered port %li task %li\n", message->internal_id, sender);
    }

    react_data(ptr, message->data, message_size - sizeof(IPC_PS2_Notify_Data));
}

void react_data(struct port_list_node *port, const char* data, size_t data_size)
{
    for (size_t i = 0; i < data_size; ++i)
        react_byte(port, data[i]);
}

void react_byte(struct port_list_node *port, unsigned char data)
{
    switch (port->state) {
    case PORT_STATE_RESET:
        // Device should respond ack and then SELF_TEST_OK or SELF_TEST_FAILURE
        // Ignore all other messages
        switch (data) {
        case RESPONSE_FAILURE:
            printf("[PS2d] Warning: Device %li PID %li reported failure during self-test...\n", port->port_id, port->owner_pid);
            break;

        case RESPONSE_SELF_TEST_OK:
            port->state = PORT_STATE_DISABLE_SCANNING;
            send_byte_port(port, COMMAND_DISABLE_SCANNING);
            port_start_timer(port, 1000);
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
            send_byte_port(port, COMMAND_IDENTIFY);
            break;
        }

        port->device_id |= data << (port->device_id_size * 8);
        port->device_id_size++;
        break;

            case PORT_STATE_WAIT:
        // Do nothing
        break;

    case PORT_STATE_DISABLE_SCANNING:
        switch (data) {
        case RESPONSE_ACK:
            // Disabled scanning. Start the identify secuence
            port->state = PORT_STATE_IDENTIFY;
            send_byte_port(port, COMMAND_IDENTIFY);
            port_start_timer(port, 1000);
            break;
        case RESPONSE_RESEND:
            send_byte_port(port, COMMAND_DISABLE_SCANNING);
            break;

        default:
            // Ignore everything until scanning is disabled
            break;
        }
        break;

    case PORT_STATE_MANAGED:
        port->managed_react_data(port, data);
        break;

        
    }
}

void port_react_timer(struct port_list_node *port, uint64_t timer_id)
{
    if (port->last_timer != timer_id)
        return;

    switch (port->state) {
    case PORT_STATE_RESET:
        // Do nothing
        break;

    case PORT_STATE_DISABLE_SCANNING:
        fprintf(stderr, "[PS2d] Warning: Did not recieve the ACK for DISABLE_SCANNING command for port %li PID %li. Resetting the port...\n", port->port_id, port->owner_pid);
        port->state = PORT_STATE_WAIT;
        port_start_timer(port, 5000);
        break;
    
    case PORT_STATE_WAIT:
        port->state = PORT_STATE_RESET;
        reset_port(port);
        break;

    case PORT_STATE_IDENTIFY:
        // Init port
        configure_port(port);
        break;

    case PORT_STATE_MANAGED:
        port->managed_react_timer(port);
        break;
    }
}

uint64_t tmr_index = 0;

uint64_t port_start_timer(struct port_list_node *port, unsigned time_ms)
{
    IPC_Start_Timer tmr = {
        .type = IPC_Start_Timer_NUM,
        .ms = time_ms,
        .reply_port = main_port,

        .extra0 = port->owner_pid,
        .extra1 = port->port_id,
        .extra2 = tmr_index,
    };

    result_t result = send_message_port(devicesd_port, sizeof(tmr), (char*)&tmr);
    if (result != SUCCESS) {
        printf("[PS2d] Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    port->last_timer = tmr_index;

    //printf("Started timer index %i\n", tmr_index);

    return tmr_index++;
}

void react_timer(IPC_Timer_Reply *tmr)
{
    struct port_list_node *ptr = list_get(tmr->extra0, tmr->extra1);

    if (ptr == NULL) {
        fprintf(stderr, "[PS2d] Warning: Recieved unrecognized timer message with data 0x%lx 0x%lx 0x%lx\n", tmr->extra0, tmr->extra1, tmr->extra2);
        return;
    }

    port_react_timer(ptr, tmr->extra2);
}

void reset_port(struct port_list_node *n)
{
    n->state = PORT_STATE_RESET;
    n->type = DEVICE_TYPE_UNKNOWN;
    n->device_id_size = 0;
    send_byte_port(n, COMMAND_RESET);
}

