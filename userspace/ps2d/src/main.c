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

#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <pmos/pmbus_helper.h>

pmos_port_t main_port = 0;
pmos_right_t main_right = INVALID_RIGHT;
struct pmos_msgloop_data msgloop_data;

pmos_port_t configuration_port = 0;

struct pmbus_helper *main_pmbus_helper = NULL;

int default_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *)
{

    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "[PS2d] Error: Message too small (size %" PRIi64 ")\n", desc->size);
        return 0;
    }

    switch (IPC_TYPE(buff)) {
    case IPC_Timer_Reply_NUM: {
        if (desc->size < sizeof(IPC_Timer_Reply)) {
            fprintf(stderr, "[PS2d] Warning: Recieved IPC_Timer_Reply of unexpected size %" PRIu64 "\n",
                    desc->size);
            break;
        }

        IPC_Timer_Reply *tmr = (IPC_Timer_Reply *)buff;

        react_timer(tmr);

        break;
    }
    default:
        fprintf(stderr, "[PS2d] Warning: Recieved message of unknown type %" PRIu32 " (right %" PRIu64 "\n", IPC_TYPE(buff), desc->sent_with_right);
        break;
    }

    return PMOS_MSGLOOP_CONTINUE;
}

pmos_right_t right_to_device = INVALID_RIGHT;

void usage(char *name)
{
    printf("ps2d Usage: %s --right-id <ID of the right to the ps2d controller>\n", name);
    exit(1);
}

void parse_args(int argc, char **argv)
{
    if (argc < 1)
        printf("ps2d: empty argc!\n");
    char *name = argv[0];

    bool have_right = false;

    for (int i = 1; i < argc; ++i) {
        char *r = argv[i];

        if (!strcmp(r, "--right-id")) {
            if (have_right) {
                printf("ps2d: Repeated --right-id\n");
                usage(name);
            }

            ++i;
            if (i >= argc) {
                printf("ps2d missing right ID\n");
                usage(name);
            }
            right_to_device = strtoull(argv[i], NULL, 0);
            have_right = true;
        } else {
            printf("Unrecognized argument %s\n", argv[i]);
            usage(name);
        }
    }

    if (!have_right) {
        printf("ps2d: no right_id!\n");
        usage(name);
    }
}

int port_msg_callback(Message_Descriptor *desc, void *buff, pmos_right_t *reply_right,
                     pmos_right_t *extra_rights, void *ctx, struct pmos_msgloop_data *)
{
    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "[PS2d] Error: Message too small (size %" PRIi64 ")\n", desc->size);
        return 0;
    }

    switch (IPC_TYPE(buff)) {
    case IPC_PS2_Notify_Data_NUM: {
        if (desc->size < sizeof(IPC_PS2_Notify_Data)) {
            fprintf(stderr, "[PS2d] Warning: Recieved IPC_PS2_Notify_Data of unexpected size %lx\n",
                    desc->size);
            break;
        }

        IPC_PS2_Notify_Data *d = (IPC_PS2_Notify_Data *)buff;

        react_data(d->data, desc->size - sizeof(*d));

        break;
    }

    default:
        fprintf(stderr, "[PS2d] Warning: Recieved message of unknown type %" PRIu32 " from the port...\n", IPC_TYPE(buff));
        break;
    }
}

pmos_right_t port_receive_right = INVALID_RIGHT;
pmos_msgloop_tree_node_t port_entry;

pmos_right_t port_send_right = INVALID_RIGHT;

void register_with_controller()
{
    right_request_t right = create_right(main_port, &port_receive_right, 0);
    if (right.result) {
        fprintf(stderr, "[PS2d] Failed to create right for the port: %i (%s)\n", (int)right.result, strerror(-(int)right.result));
        exit(1);
    }

    pmos_msgloop_node_set(&port_entry, port_receive_right, port_msg_callback, &msgloop_data);
    pmos_msgloop_insert(&msgloop_data, &port_entry);

    IPC_PS2_Reg_Port r = {
        .type = IPC_PS2_Reg_Port_NUM,
        .flags = 0,
    };

    message_extra_t extra = {
        .extra_rights = {right.right, 0},
    };
    right_request_t result = send_message_right(right_to_device, configuration_port, &r, sizeof(r), &extra, 0);
    if (result.result) {
        fprintf(stderr, "[PS2d] Failed to request the port from the device: %i (%s)\n", (int)result.result, strerror(-(int)result.result));
        exit(1);
    }

    Message_Descriptor desc;
    uint8_t *message;
    pmos_right_t extra_rights[4] = {};

    result_t get_result = get_message(&desc, &message, configuration_port, NULL, extra_rights);
    if (get_result) {
        fprintf(stderr, "[PS2d] Error getting message in the regoster request: %i (%s)\n", (int)get_result, strerror(-(int)get_result));
        exit(1);
    }

    if (desc.size < sizeof(IPC_PS2_Reg_Port_Reply)) {
        fprintf(stderr, "[PS2d] Reg port reply too small! Size: %u\n", (unsigned)desc.size);
        exit(1);
    }

    IPC_PS2_Reg_Port_Reply *reply = (IPC_PS2_Reg_Port_Reply *)message;
    if (reply->type != IPC_PS2_Reg_Port_Reply_NUM) {
        fprintf(stderr, "[PS2d] Unknown message type in reg reply: %" PRIu32 "\n", reply->type);
        exit(1);
    }

    if (reply->result) {
        fprintf(stderr, "[PS2d] Error registering PS/2 port: %i (%s)\n", reply->result, strerror(-reply->result));
        exit(1);
    }

    port_send_right = extra_rights[0];
    if (!port_send_right) {
        fprintf(stderr, "[PS2d] Recieved no send right in the PS/2 register reply\n");
        exit(1);
    }

    for (int i = 1; i < 4; ++i)
        delete_right(extra_rights[i]);

    printf("PS2d: Registered myself with the controller...\n");
}

int main(int argc, char *argv[])
{
    parse_args(argc, argv);
    printf("Hello from PS2d! My PID: %" PRIi64 ", right to device: %"PRIu64 "\n", get_task_id(), right_to_device);
    {
        ports_request_t req;

        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[PS2d] Error creating port %li\n", req.result);
            return 0;
        }
        main_port = req.port;

        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[PS2d] Error creating port %li\n", req.result);
            return 0;
        }
        configuration_port = req.port;

    }

    pmos_msgloop_initialize(&msgloop_data, main_port);

    main_pmbus_helper = pmbus_helper_create(&msgloop_data);
    if (!main_pmbus_helper) {
        fprintf(stderr, "[PS2d] Error creating pmbus helper!\n");
        return 1;
    }

    register_with_controller();

    {
        right_request_t r = create_right(main_port, &main_right, 0);
        if (r.result) {
            printf("Error %i creating right\n", (int)r.result);
            return 1;
        }
    }

    start_port();

    pmos_msgloop_tree_node_t n;
    pmos_msgloop_node_set(&n, 0, default_callback, &msgloop_data);
    pmos_msgloop_insert(&msgloop_data, &n);
    pmos_msgloop_loop(&msgloop_data);
}