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

#include <acpi/acpi.h>
#include <configuration.h>
#include <dtb/dtb.h>
#include <interrupts/interrupts.h>
#include <kernel/block.h>
#include <kernel/types.h>
#include <pci/pci.h>
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/special.h>
#include <pmos/system.h>
#include <pthread.h>
#include <serial.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <timers/hpet.h>
#include <timers/timers.h>
#include <pmos/pmbus_helper.h>

char *exec = NULL;

void usage()
{
    printf("Usage: %s [args] - initializes and manages devices in pmOS.\n"
           " Supported arguments:\n"
           "   --rsdp-desc <addr> - sets RSDP physical address\n"
           "   --rsdp20-desc <addr> - sets RSDP 2.0 physical address\n",
           exec ? exec : "");
    exit(0);
}

#define CONTROL_PORT 10

pmos_port_t main_port          = 0;
pmos_port_t configuration_port = 0;

const char *devicesd_port_name = "/pmos/devicesd";

void request_pci_devices(Message_Descriptor *desc, IPC_Request_PCI_Devices *d,
                         pmos_right_t reply_right);
void publish_object_reply(Message_Descriptor *desc, IPC_BUS_Publish_Object_Reply *r);

void init_acpi();

void *shutdown_thread(void *);

pmos_right_t main_recieve_right = 0;

int default_callback(Message_Descriptor *desc, void *msg_buff, pmos_right_t *reply_right,
                     pmos_right_t *other_rights, void *, struct pmos_msgloop_data *)
{
    if (desc->size >= sizeof(IPC_Generic_Msg)) {
        switch (((IPC_Generic_Msg *)msg_buff)->type) {
        case IPC_Request_Int_NUM: {
            if (desc->size != sizeof(IPC_Request_Int))
                printf("[devicesd] Warning: Message from PID %lx does no have the right size"
                       "(%lx)\n",
                       desc->sender, desc->size);
            // TODO: Add more checks & stuff

            IPC_Request_Int *m = (IPC_Request_Int *)msg_buff;

            request_interrupts_for(desc, m, *reply_right);
            *reply_right = 0;
        } break;
        // case IPC_Start_Timer_NUM: {
        //     if (desc->size != sizeof(IPC_Start_Timer))
        //         printf("[devicesd] Warning: Message from PID %lx does no have the right size
        //         (%lx)\n", desc->sender, desc->size);

        //     IPC_Start_Timer* m = (IPC_Start_Timer*)msg_buff;
        //     start_timer(m, desc->sender);
        // }

        //     break;
        // case IPC_Kernel_Interrupt_NUM:
        //     // TODO: Check that it's from kernel, etc.

        //     hpet_int();
        //     break;
        case IPC_Request_Serial_NUM: {
            IPC_Request_Serial *m = (IPC_Request_Serial *)msg_buff;
            request_serial(desc, m, *reply_right);
            *reply_right = 0;
        } break;
        case IPC_Request_PCI_Devices_NUM:
            request_pci_devices(desc, (IPC_Request_PCI_Devices *)msg_buff, *reply_right);
            *reply_right = 0;
            break;
        // case IPC_Kernel_Named_Port_Notification_NUM:
        //     named_port_notification(desc, (IPC_Kernel_Named_Port_Notification *)msg_buff,
        //                             other_rights[0]);
        //     other_rights[0] = 0;
        //     break;
        // default:
            printf("[devicesd] Warning: Recieved unknown message %x from PID %li\n",
                   ((IPC_Generic_Msg *)msg_buff)->type, desc->sender);
            break;
        }
    }

    return PMOS_MSGLOOP_CONTINUE;
}

struct pmos_msgloop_data main_msgloop_data;
struct pmbus_helper *pmbus_helper = NULL;

int main(int , char **)
{
    printf("Hello from devicesd!. My PID: %lx\n", get_task_id());

    // parse_args(argc, argv);

#if defined(__x86_64__) || defined(__i386__)
    pmos_request_io_permission();
#endif
    // request_priority(0);
    pmos_right_t recieve_right;

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

        auto right_req = create_right(req.port, &main_recieve_right, 0);
        if (right_req.result) {
            printf("Error creating right %i\n", (int)req.result);
            return 0;
        }
        recieve_right = right_req.right;
    }

    pmos_msgloop_initialize(&main_msgloop_data, main_port);

    pmbus_helper = pmbus_helper_create(&main_msgloop_data);
    if (!pmbus_helper) {
        printf("Failed to initialize pmbus helper!\n");
        return 0;
    }


    init_dtb();
    init_acpi();
    init_serial();

    // init_ioapic();
    // init_timers();

    // TODO: Works, but needs PCI initialization first
    // if (acpi_revision != -1)
    //    init_lai();

    // Get messages

    {
        result_t r = name_right(recieve_right, devicesd_port_name, strlen(devicesd_port_name), 0);
        if (r != SUCCESS) {
            printf("Error %i naming right\n", (int)r);
            return 0;
        }
    }

    // --------------------------------------------------
    // pthread_t thread;
    // pthread_create(&thread, NULL, shutdown_thread, NULL);
    // pthread_detach(thread);
    // --------------------------------------------------

    pmos_msgloop_tree_node_t n;
    pmos_msgloop_node_set(&n, 0, default_callback, &main_msgloop_data);
    // pmos_msgloop_node_set(&n, main_recieve_right, default_callback, &main_msgloop_data);
    pmos_msgloop_insert(&main_msgloop_data, &n);
    pmos_msgloop_loop(&main_msgloop_data);

    printf("devicesd main return 0\n");
    return 0;
}