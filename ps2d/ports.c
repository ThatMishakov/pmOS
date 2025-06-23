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

struct port_list_node *first = NULL;
struct port_list_node *last  = NULL;

void start_port(struct port_list_node *port)
{
    port->state      = PORT_STATE_RESET;
    port->last_timer = 0;

    unsigned char data[] = {0xff};
    send_data_port(port, data, sizeof(data));
}

static int react_message(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                         pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *nnode)
{
    (void)reply_right;
    (void)nnode;
    (void)extra_rights;

    struct port_list_node *node = ctx;
    uint64_t message_size       = desc->size;
    if (message_size < sizeof(IPC_Generic_Msg)) {
        printf("[PS2d] Warning: Message from port %" PRIi64 " too small! (size %zu)\n", node->index,
               (size_t)message_size);
        return 0;
    }

    uint32_t message_type = ((IPC_Generic_Msg *)buff)->type;
    switch (message_type) {
    case IPC_PS2_Notify_Data_NUM:
        if (message_size < sizeof(IPC_PS2_Notify_Data)) {
            fprintf(stderr,
                    "[PS2d] Warning: Recieved IPC_PS2_Notify_Data of unexpected size %" PRIu64 "\n",
                    message_size);
            break;
        }

        react_data(node, ((IPC_PS2_Notify_Data *)buff)->data,
                   message_size - sizeof(IPC_PS2_Notify_Data));
        break;
    default:
        fprintf(stderr,
                "[PS2d] Warning: Recieved unknown message of type 0x%" PRIx32 " from port %" PRIu64
                "\n",
                message_type, node->port_id);
        break;
    }

    return 0;
}

extern pmos_port_t main_port;
bool register_port(IPC_PS2_Reg_Port *message, uint64_t sender, struct pmos_msgloop_data *msgloop,
                   pmos_right_t reply_right, pmos_right_t main_right,
                   pmos_right_t configuration_right)
{
    struct port_list_node *ptr     = NULL;
    pmos_msgloop_tree_node_t *node = NULL;
    int status                     = 0;
    pmos_right_t recieve_right     = INVALID_RIGHT;
    pmos_right_t send_right        = INVALID_RIGHT;

    IPC_PS2_Config reply = {
        .type         = IPC_PS2_Config_NUM,
        .flags        = 0,
        .internal_id  = message->internal_id,
        .request_type = IPC_PS2_Config_Reg_Port,
        .result       = 0,
    };

    ptr = calloc(sizeof(struct port_list_node), 1);
    if (ptr == NULL) {
        status = -ENOMEM;
        goto error_status;
    }

    node = malloc(sizeof(*node));
    if (!ptr) {
        status = -ENOMEM;
        goto error_status;
    }

    right_request_t r = create_right(main_port, &recieve_right, 0);
    if (r.result) {
        status = (int)r.result;
        goto error_status;
    }
    send_right = r.right;

    static uint64_t port_index_counter = 0;

    ptr->com_right      = main_right;
    ptr->last_timer     = 0;
    ptr->index          = ++port_index_counter;
    ptr->state          = 0;
    ptr->state          = PORT_STATE_RESET;
    ptr->type           = DEVICE_TYPE_UNKNOWN;
    ptr->device_id_size = 0;
    ptr->port_id        = message->internal_id;
    ptr->next           = NULL;
    ptr->previous       = NULL;
    ptr->node           = node;

    message_extra_t rr = {
        .extra_rights  = {send_right},
        .memory_object = 0,
    };

    result_t result;
    if ((result = send_message_right(reply_right, 0, (char *)&reply, sizeof(reply), &rr,
                                     SEND_MESSAGE_DELETE_RIGHT)
                      .result) != SUCCESS) {
        printf("[PS2d] Warning: Could not reply to config request to task %" PRIu64 "\n", sender);
        goto error;
    }

    pmos_msgloop_node_set(node, recieve_right, react_message, ptr);
    pmos_msgloop_insert(msgloop, node);
    list_push_back(ptr);
    start_port(ptr);

    return true;
error_status:

    assert(status);
    reply.result = status;

    result_t rrr = send_message_right(reply_right, 0, &reply, sizeof(reply), NULL, 0).result;
    if (!rrr)
        reply_right = INVALID_RIGHT;

error:
    delete_right(reply_right);
    delete_right(main_right);
    delete_right(configuration_right);
    free(ptr);
    free(node);
    delete_right(send_right);
    return false;
}

int send_data_port(struct port_list_node *port, const unsigned char *data, size_t data_size)
{
    size_t struct_size     = sizeof(IPC_PS2_Send_Data) + data_size;
    IPC_PS2_Send_Data *str = alloca(struct_size);

    str->type        = IPC_PS2_Send_Data_NUM;
    str->flags       = 0;
    str->internal_id = port->port_id;
    memcpy(str->data, data, data_size);

    return (int)send_message_right(port->com_right, 0, (const char *)str, struct_size, NULL, 0).result;
}

int send_byte_port(struct port_list_node *port, unsigned char byte)
{
    return send_data_port(port, &byte, sizeof(byte));
}

struct port_list_node *list_find_by_timer(uint64_t timer_id)
{
    struct port_list_node *ptr = first;
    while (ptr != NULL && ptr->last_timer != timer_id)
        ptr = ptr->next;

    return ptr;
}

void list_push_back(struct port_list_node *n)
{
    if (last != NULL) {
        last->next  = n;
        n->previous = last;
        n->next     = NULL;
        last        = n;
    } else {
        n->previous = NULL;
        n->next     = NULL;
        first       = n;
        last        = n;
    }
}

void react_data(struct port_list_node *port, const char *data, size_t data_size)
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
            printf("[PS2d] Warning: Port %" PRIu64 " reported failure during self-test...\n",
                   port->index);
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
        fprintf(stderr,
                "[PS2d] Warning: Did not recieve the ACK for DISABLE_SCANNING command for port %" PRIu64
                ". Resetting the port...\n",
                port->index);
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
    int result = pmos_request_timer(main_port, time_ms * 1000000, tmr_index);
    if (result != SUCCESS) {
        printf("[PS2d] Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    port->last_timer = tmr_index;

    // printf("Started timer index %i\n", tmr_index);

    return tmr_index++;
}

void react_timer(IPC_Timer_Reply *tmr)
{
    struct port_list_node *ptr = list_find_by_timer(tmr->timer_id);

    if (ptr == NULL) {
        printf("[PS2d] Warning: Recieved timer reply %li for unknown port\n", tmr->timer_id);
        return;
    }

    port_react_timer(ptr, tmr->timer_id);
}

void reset_port(struct port_list_node *n)
{
    n->state          = PORT_STATE_RESET;
    n->type           = DEVICE_TYPE_UNKNOWN;
    n->device_id_size = 0;
    send_byte_port(n, COMMAND_RESET);
}
