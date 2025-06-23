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

pmos_port_t main_port = 0;

pmos_right_t devicesd_right = 0;

const char *ps2d_port_name = "/pmos/ps2d";

pmos_right_t main_right = INVALID_RIGHT;

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

    case IPC_PS2_Reg_Port_NUM: {
        if (desc->size < sizeof(IPC_PS2_Reg_Port)) {
            fprintf(stderr, "[PS2d] Warning: Recieved IPC_PS2_Reg_Port of unexpected size %" PRIu64 "\n",
                    desc->size);
            break;
        }

        IPC_PS2_Reg_Port *d = (IPC_PS2_Reg_Port *)buff;

        bool result = register_port(d, desc->sender, ctx, *reply_right, extra_rights[0], extra_rights[1]);
        *reply_right = 0;
        extra_rights[0] = INVALID_RIGHT;
        extra_rights[1] = INVALID_RIGHT;

        if (result)
            printf("[PS2d] Info: Task %" PRIu64 " registered port %" PRIu64 "\n", desc->sender, d->internal_id);

        break;
    }

    // case IPC_PS2_Notify_Data_NUM: {
    //     if (desc.size < sizeof(IPC_PS2_Notify_Data)) {
    //         fprintf(stderr, "[PS2d] Warning: Recieved IPC_PS2_Notify_Data of unexpected size %lx\n",
    //                 desc.size);
    //         break;
    //     }

    //     IPC_PS2_Notify_Data *d = (IPC_PS2_Notify_Data *)buff;

    //     react_message(d, desc.sender, desc.size);

    //     break;
    // }

    default:
        fprintf(stderr, "[PS2d] Warning: Recieved message of unknown type %" PRIu32 "\n", IPC_TYPE(buff));
        break;
    }
}

int main()
{
    printf("Hello from PS2d! My PID: %li\n", get_task_id());
    {
        ports_request_t req;

        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[PS2d] Error creating port %li\n", req.result);
            return 0;
        }
        main_port = req.port;
    }

    {
        right_request_t r = create_right(main_port, &main_right, 0);
        if (r.result) {
            printf("Error %i creating right\n", (int)r.result);
            return 1;
        }

        result_t rr = name_right(r.right, ps2d_port_name, strlen(ps2d_port_name), 0);
        if (rr != SUCCESS) {
            printf("Error %i naming port\n", (int)rr);
            return 1;
        }
    }

    struct pmos_msgloop_data data;
    pmos_msgloop_initialize(&data, main_port);

    pmos_msgloop_tree_node_t n;
    pmos_msgloop_node_set(&n, 0, default_callback, &data);
    pmos_msgloop_insert(&data, &n);
    pmos_msgloop_loop(&data);
}