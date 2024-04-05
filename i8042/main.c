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

#include <stdio.h>
#include "io.h"
#include "registers.h"
#include <stdbool.h>
#include <stdlib.h>
#include <pmos/system.h>
#include <kernel/block.h>
#include <pmos/helpers.h>
#include "ports.h"
#include <pmos/ipc.h>
#include "timers.h"
#include <pmos/ports.h>
#include <string.h>
#include <stdlib.h>

bool has_second_channel = false;

bool first_port_works =  false;
bool second_port_works = false;

bool enable_first_channel = true;
bool enable_second_channel = true;

pmos_port_t main_port = 0;
pmos_port_t configuration_port = 0;

pmos_port_t devicesd_port = 0;
pmos_port_t ps2d_port = 0;

uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_port)
{
    uint8_t int_vector = 0;
    unsigned long mypid = getpid();

    IPC_Reg_Int m = {IPC_Reg_Int_NUM, IPC_Reg_Int_FLAG_EXT_INTS, intnum, 0, mypid, int_port, configuration_port};
    result_t result = send_message_port(devicesd_port, sizeof(m), (char*)&m);
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    result = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not get message\n");
        return 0;
    }

    if (desc.size < sizeof(IPC_Reg_Int_Reply)) {
        printf("[i8042] Warning: Recieved message which is too small\n");
        free(message);
        return 0;
    }

    IPC_Reg_Int_Reply* reply = (IPC_Reg_Int_Reply*)message;

    if (reply->type != IPC_Reg_Int_Reply_NUM) {
        printf("[i8042] Warning: Recieved unexepcted message type\n");
        free(message);
        return 0;
    }

    if (reply->status == 0) {
        printf("[i8042] Warning: Did not assign the interrupt\n");
    } else {
        int_vector = reply->intno;
        printf("[i8042] Info: Assigned interrupt %i\n", int_vector);
    }
    free(message);
    return int_vector;
}

pmos_port_t register_port(unsigned char id)
{
    IPC_PS2_Reg_Port req = {
        .type = IPC_PS2_Reg_Port_NUM,
        .flags = 0,
        .internal_id = id,
        .cmd_port = main_port,
        .config_port = configuration_port,
    };

    result_t result = send_message_port(ps2d_port, sizeof(req), (char*)&req);
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to register the port %i\n", id);
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    result = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not get message\n");
        return 0;
    }

    if (desc.size < sizeof(IPC_PS2_Config)) {
        printf("[i8042] Warning: Recieved message which is too small\n");
        free(message);
        return 0;
    }

    IPC_PS2_Config* reply = (IPC_PS2_Config *)message;

    if (reply->type != IPC_PS2_Config_NUM) {
        printf("[i8042] Warning: Recieved unexepcted message type\n");
        free(message);
        return 0;
    }

    if (reply->request_type != IPC_PS2_Config_Reg_Port) {
        printf("[i8042] Warning: Could register port %i\n", id);
        free(message);
        return 0; 
    }

    if (reply->result_cmd == 0)
        printf("[i8042] Warning: Did not register the port\n");

    pmos_port_t port = reply->result_cmd;
    free(message);

    return port;
};

void register_ports()
{
    bool success = false;

    if (first_port_works)
        success = (ports[0].notification_port = register_port(0));

    if (second_port_works)
        ports[1].notification_port = register_port(1);

    success = success || ports[1].notification_port;

    if (!success) {
        fprintf(stderr, "[i8042] Error: Could not register ports with PS2d\n");
        exit(1);
    }
}

void init_controller()
{
    printf("[i8042] Initializing the controller...\n");
    uint8_t status;
    uint8_t data;
    uint8_t config_byte;

    // Disable ports
    outb(RW_PORT, DISABLE_FIRST_PORT);
    outb(RW_PORT, DISABLE_SECOND_PORT);

    // Clear output buffer
    data = inb(DATA_PORT); 

    // Configure the controller
    outb(RW_PORT, CMD_CONFIG_READ);
    config_byte = inb(DATA_PORT);

    // printf("PS/2 config value %x\n", config_byte);

    // Disables translation and enables first and second ports' interrupts
    config_byte &= ~0x43;
    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, config_byte);

    if (config_byte & 0x20) {
        has_second_channel = true; 
        printf("[i8042] Info: PS/2 controller has second channel\n");
    }

    // Perform self-test
    outb(RW_PORT, TEST_CONTROLLER);
    do {
        status = inb(RW_PORT);
    } while (!(status & STATUS_MASK_OUTPUT_FULL));
    data = inb(DATA_PORT);
    if (data != 0x55) {
        printf("[i8042] Error: PS/2 controller did not pass self-test (status %x)\n", data);
        exit(1);
    }

    // Restore configuration bit
    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, config_byte);

    // Test if the controller has second channel
    if (has_second_channel) {
        outb(RW_PORT, ENABLE_SECOND_PORT);
        data = inb(DATA_PORT);

        if (data & 0x20)
            has_second_channel = false;
        else {
            outb(RW_PORT, DISABLE_SECOND_PORT);
        }
    }

    outb(RW_PORT, TEST_FIRST_PORT);
    do {
        status = inb(RW_PORT);
    } while (!(status & STATUS_MASK_OUTPUT_FULL));
    data = inb(DATA_PORT);
    if (!data) {
        printf("[i8042] Info: Port 1 passed self-test\n");
        first_port_works = enable_first_channel;
    } else {
        printf("[i8042] Notice: first port didn't pass self-test (error %x)\n", data);
    }

    if (has_second_channel) {
        outb(RW_PORT, TEST_SECOND_PORT);
        do {
            status = inb(RW_PORT);
        } while (!(status & STATUS_MASK_OUTPUT_FULL));
        data = inb(DATA_PORT);
        if (!data) {
            printf("[i8042] Info: Port 2 passed self-test\n");
            second_port_works = enable_second_channel;
        } else {
            printf("[i8042] Notice: second port didn't pass self-test (error %x)\n", data);
        }
    }
}

void enable_ports()
{
    uint8_t data;
    outb(RW_PORT, CMD_CONFIG_READ);
    data = inb(DATA_PORT);

    if (first_port_works) data |= 0x01;
    if (second_port_works) data |= 0x02;

    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, data);

    if (first_port_works)
        outb(RW_PORT, ENABLE_FIRST_PORT);

    if (second_port_works)
        outb(RW_PORT, ENABLE_SECOND_PORT);
}

uint8_t port1_int = 0;
uint8_t port2_int = 0;

int main()
{
    {
        ports_request_t req;
        req = create_port(PID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[i8042] Error creating port %li\n", req.result);
            return 0;
        }
        configuration_port = req.port;

        req = create_port(PID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[i8042] Error creating port %li\n", req.result);
            return 0;
        }
        main_port = req.port;

        static const char* devicesd_port_name = "/pmos/devicesd";
        ports_request_t devicesd_port_req = get_port_by_name(devicesd_port_name, strlen(devicesd_port_name), 0);
        if (devicesd_port_req.result != SUCCESS) {
            printf("[i8042] Warning: Could not get devicesd port. Error %li\n", devicesd_port_req.result);
            return 0;
        }
        devicesd_port = devicesd_port_req.port;

        static const char* ps2d_port_name = "/pmos/ps2d";
        ports_request_t ps2d_port_req = get_port_by_name(ps2d_port_name, strlen(ps2d_port_name), 0);
        if (ps2d_port_req.result != SUCCESS) {
            printf("[i8042] Warning: Could not get devicesd port. Error %li\n", ps2d_port_req.result);
            return 0;
        }
        ps2d_port = ps2d_port_req.port;
    }

    request_priority(1);
    init_controller();

    if (!first_port_works && !second_port_works) {
        fprintf(stderr, "[i8042] Error: No PS/2 ports apear to be useful...\n");
        exit(1);
    }

    register_ports();

    if (second_port_works)
        port2_int = get_interrupt_number(12, main_port);

    if (first_port_works)
        port1_int = get_interrupt_number(1, main_port);

    enable_ports();

    poll_ports();

    while (1) {
        result_t result;

        Message_Descriptor desc = {};
        unsigned char* message = NULL;
        result = get_message(&desc, &message, main_port);

        if (result != SUCCESS) {
            fprintf(stderr, "[i8042] Error: Could not get message\n");
        }

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "[i8042] Error: Message too small (size %li)\n", desc.size);
            free(message);
        }


        switch (IPC_TYPE(message)) {
        case IPC_Kernel_Interrupt_NUM: {
            if (desc.size < sizeof(IPC_Kernel_Interrupt)) {
                printf("[i8042] Warning: message for type %i is too small (size %lx)\n", IPC_Kernel_Interrupt_NUM, desc.size);
                break;
            }

            IPC_Kernel_Interrupt* str = (IPC_Kernel_Interrupt*)message;

            if (str->intno == port1_int) {
                react_port1_int();
                break;
            }

            if (str->intno == port2_int) {
                react_port2_int();
                break;
            }

            printf("[i8042] Warning: Recieved unknown interrupt %i\n", str->intno);
        }
            break;
        case IPC_Timer_Reply_NUM: {
            unsigned expected_size = sizeof(IPC_Timer_Reply);
            if (desc.size != expected_size) {
                fprintf(stderr, "[i8042] Warning: Recieved message of wrong size on channel %lx (expected %x got %lx)\n", desc.channel, expected_size, desc.size);
                break;
            }

            IPC_Timer_Reply* reply = (IPC_Timer_Reply*)message;

            if (reply->type != IPC_Timer_Reply_NUM) {
                fprintf(stderr, "[i8042] Warning: Recieved unexpected meesage of type %x on channel %lx\n", reply->type, desc.channel);
                break;
            }

            react_timer(reply->extra0);            

            break;
        }
        case IPC_PS2_Send_Data_NUM: {
            IPC_PS2_Send_Data* d = (IPC_PS2_Send_Data *)message;
            react_send_data(d, desc.size);
            break;
        }
        default:
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x\n", IPC_TYPE(message));
            break;
        }

        free(message);
    }

    return 0;
}