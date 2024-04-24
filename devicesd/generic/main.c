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

#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <string.h>
#include <pmos/system.h>
#include <stdio.h>
#include <acpi/acpi.h>
#include <stddef.h>
#include <pmos/special.h>
#include <pci/pci.h>
#include <ioapic/ioapic.h>
#include <pmos/ipc.h>
#include <interrupts/interrupts.h>
#include <configuration.h>
#include <timers/timers.h>
#include <timers/hpet.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <serial.h>

char* exec = NULL;

void usage()
{
    printf("Usage: %s [args] - initializes and manages devices in pmOS.\n"
            " Supported arguments:\n"
            "   --rsdp-desc <addr> - sets RSDP physical address\n"
            "   --rsdp20-desc <addr> - sets RSDP 2.0 physical address\n", exec ? exec : "");
    exit(0);
}

#define CONTROL_PORT 10

pmos_port_t main_port = 0;
pmos_port_t configuration_port = 0;

const char *devicesd_port_name = "/pmos/devicesd";

int main(int argc, char** argv) {
    printf("Hello from devicesd!. My PID: %lx\n", get_task_id());
    
    // parse_args(argc, argv);

    pmos_request_io_permission();
    request_priority(0);

    {
        ports_request_t req;
        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("Error creating port %li\n", req.result);
            return 0;
        }
        configuration_port = req.port;

        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("Error creating port %li\n", req.result);
            return 0;
        }
        main_port = req.port;
    }

    init_acpi();
    init_pci();
    init_serial();

    // init_ioapic();
    // init_timers();

    // TODO: Works, but needs PCI initialization first
    //if (acpi_revision != -1)
    //    init_lai();

    // Get messages


    {
        result_t r = name_port(main_port, devicesd_port_name, strlen(devicesd_port_name), 0);
        if (r != SUCCESS) {
            printf("Error %li naming port\n", r);
            return 0;
        }
    }


    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        char* msg_buff = (char*)malloc(msg.size);

        get_first_message(msg_buff, 0, main_port);

        if (msg.size >= sizeof(IPC_Generic_Msg)) {
            switch (((IPC_Generic_Msg*)msg_buff)->type) {
            // case IPC_Reg_Int_NUM: {
            //     if (msg.size != sizeof(IPC_Reg_Int))
            //         printf("[devicesd] Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);
            //     // TODO: Add more checks & stuff

            //     IPC_Reg_Int* m = (IPC_Reg_Int*)msg_buff;

            //     uint8_t result = configure_interrupts_for(m);

            //     IPC_Reg_Int_Reply reply;
            //     reply.type = IPC_Reg_Int_Reply_NUM;
            //     reply.status = result != 0;
            //     reply.intno = result;
            //     send_message_port(m->reply_chan, sizeof(reply), (char*)&reply);
            // }
            //     break;
            // case IPC_Start_Timer_NUM: {
            //     if (msg.size != sizeof(IPC_Start_Timer))
            //         printf("[devicesd] Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);

            //     IPC_Start_Timer* m = (IPC_Start_Timer*)msg_buff;
            //     start_timer(m, msg.sender);
            // }

            //     break;
            // case IPC_Kernel_Interrupt_NUM:
            //     // TODO: Check that it's from kernel, etc.

            //     hpet_int();
            //     break;
            case IPC_Request_Serial_NUM:
                IPC_Request_Serial * m = (IPC_Request_Serial*)msg_buff;
                request_serial(&msg, m);
                break;
            default:
                printf("[devicesd] Warning: Recieved unknown message %x from PID %li\n", ((IPC_Generic_Msg*)msg_buff)->type, msg.sender);    
                break;
            }
            }


        free(msg_buff);
    }

    return 0;
}