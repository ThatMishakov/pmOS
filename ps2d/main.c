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

#include <pmos/ipc.h>
#include <stdio.h>
#include <pmos/ports.h>
#include "ports.h"
#include <string.h>
#include <pmos/helpers.h>
#include <stdlib.h>

pmos_port_t main_port = 0;
pmos_port_t devicesd_port = 0;

const char* ps2d_port_name = "/pmos/ps2d";

int main()
{
    printf("Hello from PS2d! My PID: %li\n", getpid());
    {
        ports_request_t req;

        req = create_port(PID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[PS2d] Error creating port %li\n", req.result);
            return 0;
        }
        main_port = req.port;
    }

    {
        result_t r = name_port(main_port, ps2d_port_name, strlen(ps2d_port_name), 0);
        if (r != SUCCESS) {
            printf("Error %li naming port\n", r);
            return 0;
        }
    }

    {
        static const char* devicesd_port_name = "/pmos/devicesd";
        ports_request_t devicesd_port_req = get_port_by_name(devicesd_port_name, strlen(devicesd_port_name), 0);
        if (devicesd_port_req.result != SUCCESS) {
            printf("[i8042] Warning: Could not get devicesd port. Error %li\n", devicesd_port_req.result);
            return 0;
        }
        devicesd_port = devicesd_port_req.port;
    }

    while (1) {
        result_t result;

        Message_Descriptor desc = {};
        unsigned char* message = NULL;
        result = get_message(&desc, &message, main_port);

        if (result != SUCCESS) {
            fprintf(stderr, "[PS2d] Error: Could not get message\n");
        }

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "[PS2d] Error: Message too small (size %li)\n", desc.size);
            free(message);
        }


        switch (IPC_TYPE(message)) {
        case IPC_Timer_Reply_NUM:{
            if (desc.size < sizeof(IPC_Timer_Reply)) {
                fprintf(stderr, "[PS2d] Warning: Recieved IPC_Timer_Reply of unexpected size %lx\n", desc.size);
                break;
            }

            IPC_Timer_Reply *tmr = (IPC_Timer_Reply *)message;

            react_timer(tmr);

            break;
            }
        
        case IPC_PS2_Reg_Port_NUM: {
            if (desc.size < sizeof(IPC_PS2_Reg_Port)) {
                fprintf(stderr, "[PS2d] Warning: Recieved IPC_PS2_Reg_Port of unexpected size %lx\n", desc.size);
                break;
            }

            IPC_PS2_Reg_Port *d = (IPC_PS2_Reg_Port *)message;

            bool result = register_port(d, desc.sender);

            if (result)
                printf("[PS2d] Info: Task %li registered port %li\n", desc.sender, d->internal_id);        

            break;
            }

        case IPC_PS2_Notify_Data_NUM: {
            if (desc.size < sizeof(IPC_PS2_Notify_Data)) {
                fprintf(stderr, "[PS2d] Warning: Recieved IPC_PS2_Notify_Data of unexpected size %lx\n", desc.size);
                break;
            }

            IPC_PS2_Notify_Data *d = (IPC_PS2_Notify_Data *)message;

            react_message(d, desc.sender, desc.size);

            break;
            }

        default:
            fprintf(stderr, "[PS2d] Warning: Recieved message of unknown type %x\n", IPC_TYPE(message));
            break;
        }

        free(message);
    }

    return 0;
}