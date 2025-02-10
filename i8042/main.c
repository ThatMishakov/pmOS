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

#include "io.h"
#include "ports.h"
#include "registers.h"
#include "timers.h"

#include <errno.h>
#include <inttypes.h>
#include <kernel/block.h>
#include <pmos/special.h>
#include <pmos/helpers.h>
#include <pmos/interrupts.h>
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <pmos/system.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool has_second_channel = false;

bool first_port_works  = false;
bool second_port_works = false;

bool enable_first_channel  = true;
bool enable_second_channel = true;

pmos_port_t main_port          = 0;
pmos_port_t configuration_port = 0;

pmos_port_t devicesd_port = 0;
pmos_port_t ps2d_port     = 0;

__thread pmos_port_t control_port = 0;
pmos_port_t get_control_port()
{
    if (control_port == 0) {
        ports_request_t req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[i8042] Error creating port %" PRIu64 "\n", req.result);
            return 0;
        }
        control_port = req.port;
    }

    return control_port;
}

uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_port)
{
    pmos_port_t control_port = get_control_port();
    if (control_port == 0) {
        printf("[i8042] Error: Could not get control port\n");
        return 0;
    }

    uint8_t int_vector  = 0;
    unsigned long mypid = get_task_id();

    IPC_Reg_Int m   = {.type       = IPC_Reg_Int_NUM,
                       .flags      = IPC_Reg_Int_FLAG_EXT_INTS,
                       .intno      = intnum,
                       .int_flags  = 0,
                       .dest_task  = mypid,
                       .dest_chan  = int_port,
                       .reply_chan = control_port};
    result_t result = send_message_port(devicesd_port, sizeof(m), (char *)&m);
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char *message  = NULL;
    for (int i = 0; i < 10; i++) {
        result = get_message(&desc, &message, control_port);
        if ((int)result != -EINTR)
            break;
    }

    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not get message %i (%s)\n", (int)-result, strerror(-result));
        return 0;
    }

    if (desc.size < sizeof(IPC_Reg_Int_Reply)) {
        printf("[i8042] Warning: Recieved message which is too small (%i expected %i)\n",
               (int)desc.size, (int)sizeof(IPC_Reg_Int_Reply));
        free(message);
        return 0;
    }

    IPC_Reg_Int_Reply *reply = (IPC_Reg_Int_Reply *)message;

    if (reply->type != IPC_Reg_Int_Reply_NUM) {
        printf("[i8042] Warning: Recieved unexepcted message type\n");
        free(message);
        return 0;
    }

    if (reply->status) {
        printf("[i8042] Warning: Did not assign the interrupt, status: %i\n", (int)reply->status);
    } else {
        int_vector = reply->intno;
        printf("[i8042] Info: Assigned interrupt %i\n", int_vector);
    }
    free(message);
    return int_vector;
}

void *interrupt_thread(void *arg)
{
    ptrdiff_t port      = (ptrdiff_t)arg;
    ports_request_t req = create_port(TASK_ID_SELF, 0);
    if (req.result != SUCCESS) {
        printf("[i8042] Error creating port %" PRIi64 "\n", req.result);
        exit(1);
    }

    result_t result = pmos_request_io_permission();
    if (result) {
        printf("[i8042] Error: Could not request io permission %i\n", (int)result);
        exit(1);
    }

    pmos_port_t int_port = req.port;

    int port_pin       = port ? 2 : 12;
    uint8_t int_vector = get_interrupt_number(port_pin, int_port);
    if (int_vector == 0) {
        printf("[i8042] Error: Could not get interrupt number\n");
        exit(1);
    } else {
        printf("[i8042] Info: Got interrupt number %i\n", int_vector);
    }

    while (1) {
        result_t result;

        Message_Descriptor desc = {};
        unsigned char *message  = NULL;
        result                  = get_message(&desc, &message, int_port);

        if (result != SUCCESS) {
            fprintf(stderr, "[i8042] Error: Could not get message %i %s\n", (int)-result,
                    strerror(-result));
        }

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "[i8042] Error: Message too small (size %" PRIu64 ")\n", desc.size);
            free(message);
        }

        if (IPC_TYPE(message) != IPC_Kernel_Interrupt_NUM) {
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x\n",
                    IPC_TYPE(message));
            free(message);
            continue;
        }

        IPC_Kernel_Interrupt *kmsg = (IPC_Kernel_Interrupt *)message;
        if (kmsg->intno != int_vector) {
            fprintf(stderr, "[i8042] Warning: Recieved interrupt of unknown number %i\n",
                    kmsg->intno);
            complete_interrupt(kmsg->intno);
            free(message);
            continue;
        }

        int mutex_result = pthread_mutex_lock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
            free(message);
            complete_interrupt(kmsg->intno);
            continue;
        }

        if (IPC_TYPE(message) != IPC_Kernel_Interrupt_NUM) {
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x\n",
                    IPC_TYPE(message));
        } else {
            if (port == 0)
                react_port1_int();
            else
                react_port2_int();
        }

        mutex_result = pthread_mutex_unlock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not unlock mutex\n");
            complete_interrupt(kmsg->intno);
            exit(mutex_result);
        }
        complete_interrupt(kmsg->intno);
        free(message);
    }
}

pthread_t thread1, thread2;

void init_interrupts()
{
    if (first_port_works) {
        int result = pthread_create(&thread1, NULL, interrupt_thread, (void *)0);
        if (result < 0)
            printf("[i8042] Could not create thread for port 1: %i\n", result);
        else
            pthread_detach(thread1);
    }

    if (second_port_works) {
        int result = pthread_create(&thread2, NULL, interrupt_thread, (void *)1);
        if (result < 0)
            printf("[i8042] Could not create thread for port 2: %i\n", result);
        else
            pthread_detach(thread2);
    }
}

pmos_port_t register_port(unsigned char id)
{
    IPC_PS2_Reg_Port req = {
        .type          = IPC_PS2_Reg_Port_NUM,
        .flags         = 0,
        .internal_id   = id,
        .cmd_port      = main_port,
        .config_port   = configuration_port,
        .task_group_id = pmos_process_task_group(),
    };

    result_t result = send_message_port(ps2d_port, sizeof(req), (char *)&req);
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to register the port %i\n", id);
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char *message  = NULL;
    result                  = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not get message\n");
        return 0;
    }

    if (desc.size < sizeof(IPC_PS2_Config)) {
        printf("[i8042] Warning: Recieved message which is too small\n");
        free(message);
        return 0;
    }

    IPC_PS2_Config *reply = (IPC_PS2_Config *)message;

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

void wait_output_set()
{
    uint8_t status;
    do {
        status = inb(RW_PORT);
    } while (!(status & STATUS_MASK_OUTPUT_FULL));
}
void wait_input_clear()
{
    uint8_t status;
    do {
        status = inb(RW_PORT);
    } while (status & STATUS_MASK_INPUT_FULL);
}

void write_wait(uint32_t port, uint8_t cmd)
{
    wait_input_clear();
    outb(port, cmd);
}

uint8_t read_wait(uint32_t port)
{
    wait_output_set();
    return inb(port);
}

void init_controller()
{
    printf("[i8042] Initializing the controller...\n");
    // uint8_t status;
    uint8_t data;
    uint8_t config_byte;

    // Disable ports
    write_wait(RW_PORT, DISABLE_FIRST_PORT);
    write_wait(RW_PORT, DISABLE_SECOND_PORT);

    printf("[i8042] Info: Ports disabled\n");

    // Clear output buffer
    data = inb(DATA_PORT);

    // Configure the controller
    write_wait(RW_PORT, CMD_CONFIG_READ);
    config_byte = read_wait(DATA_PORT);

    printf("PS/2 config value %x\n", config_byte);

    // Disables translation and enables first and second ports' interrupts
    config_byte &= ~((1 << 6) | (1 << 4) | (0b11));
    write_wait(RW_PORT, CMD_CONFIG_WRITE);
    write_wait(DATA_PORT, config_byte);

    if (config_byte & 0x20) {
        has_second_channel = true;
        printf("[i8042] Info: PS/2 controller has second channel\n");
    }

    // Perform self-test
    write_wait(RW_PORT, TEST_CONTROLLER);
    data = read_wait(DATA_PORT);
    if (data != 0x55) {
        printf("[i8042] Error: PS/2 controller did not pass self-test (status %x)\n", data);
        exit(1);
    }

    // Restore configuration bit
    write_wait(RW_PORT, CMD_CONFIG_WRITE);
    write_wait(DATA_PORT, config_byte);

    // Test if the controller has second channel
    if (has_second_channel) {
        write_wait(RW_PORT, ENABLE_SECOND_PORT);
        write_wait(RW_PORT, CMD_CONFIG_READ);
        data = read_wait(DATA_PORT);

        if (data & 0x20)
            has_second_channel = false;
        else {
            write_wait(RW_PORT, DISABLE_SECOND_PORT);
        }
    }

    write_wait(RW_PORT, TEST_FIRST_PORT);
    data = read_wait(DATA_PORT);
    if (!data) {
        printf("[i8042] Info: Port 1 passed self-test\n");
        first_port_works = enable_first_channel;
    } else {
        printf("[i8042] Notice: first port didn't pass self-test (error %x)\n", data);
    }

    if (has_second_channel) {
        write_wait(RW_PORT, TEST_SECOND_PORT);
        data = read_wait(DATA_PORT);
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
    if (first_port_works)
        write_wait(RW_PORT, ENABLE_FIRST_PORT);

    if (second_port_works)
        write_wait(RW_PORT, ENABLE_SECOND_PORT);

    uint8_t data;
    write_wait(RW_PORT, CMD_CONFIG_READ);
    data = read_wait(DATA_PORT);

    if (first_port_works)
        data |= 0x01;
    if (second_port_works)
        data |= 0x02;

    write_wait(RW_PORT, CMD_CONFIG_WRITE);
    write_wait(DATA_PORT, data);
}

uint8_t port1_int = 0;
uint8_t port2_int = 0;

pthread_mutex_t ports_mutex = PTHREAD_MUTEX_INITIALIZER;

int main()
{
    {
        ports_request_t req;
        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[i8042] Error creating port %" PRIi64 "\n", req.result);
            return 0;
        }
        configuration_port = req.port;

        req = create_port(TASK_ID_SELF, 0);
        if (req.result != SUCCESS) {
            printf("[i8042] Error creating port %" PRIi64 "\n", req.result);
            return 0;
        }
        main_port = req.port;

        static const char *devicesd_port_name = "/pmos/devicesd";
        ports_request_t devicesd_port_req =
            get_port_by_name(devicesd_port_name, strlen(devicesd_port_name), 0);
        if (devicesd_port_req.result != SUCCESS) {
            printf("[i8042] Warning: Could not get devicesd port. Error %" PRIi64 "\n",
                   devicesd_port_req.result);
            return 0;
        }
        devicesd_port = devicesd_port_req.port;

        static const char *ps2d_port_name = "/pmos/ps2d";
        ports_request_t ps2d_port_req = get_port_by_name(ps2d_port_name, strlen(ps2d_port_name), 0);
        if (ps2d_port_req.result != SUCCESS) {
            printf("[i8042] Warning: Could not get devicesd port. Error %" PRIi64 "\n",
                   ps2d_port_req.result);
            return 0;
        }
        ps2d_port = ps2d_port_req.port;
    }

    pmos_request_io_permission();
    request_priority(1);
    init_controller();

    if (!first_port_works && !second_port_works) {
        fprintf(stderr, "[i8042] Error: No PS/2 ports apear to be useful...\n");
        exit(1);
    }

    register_ports();

    init_interrupts();

    enable_ports();

    poll_ports();

    while (1) {
        result_t result;

        Message_Descriptor desc = {};
        unsigned char *message  = NULL;
        result                  = get_message(&desc, &message, main_port);

        if (result != SUCCESS) {
            fprintf(stderr, "[i8042] Error: Could not get message\n");
        }

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "[i8042] Error: Message too small (size %" PRIu64 ")\n", desc.size);
            free(message);
        }

        int mutex_result = pthread_mutex_lock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
            free(message);
            continue;
        }

        switch (IPC_TYPE(message)) {
        case IPC_Kernel_Interrupt_NUM: {
            if (desc.size < sizeof(IPC_Kernel_Interrupt)) {
                printf("[i8042] Warning: message for type %i is too small (size %" PRIu64 ")\n",
                       IPC_Kernel_Interrupt_NUM, desc.size);
                break;
            }

            IPC_Kernel_Interrupt *str = (IPC_Kernel_Interrupt *)message;

            if (str->intno == port1_int) {
                react_port1_int();
                break;
            }

            if (str->intno == port2_int) {
                react_port2_int();
                break;
            }

            printf("[i8042] Warning: Recieved unknown interrupt %i\n", str->intno);
        } break;
        case IPC_Timer_Reply_NUM: {
            unsigned expected_size = sizeof(IPC_Timer_Reply);
            if (desc.size != expected_size) {
                fprintf(stderr,
                        "[i8042] Warning: Recieved message of wrong size on port (expected "
                        "%x got %" PRIu64 ")\n",
                        expected_size, desc.size);
                break;
            }

            IPC_Timer_Reply *reply = (IPC_Timer_Reply *)message;

            if (reply->type != IPC_Timer_Reply_NUM) {
                fprintf(stderr, "[i8042] Warning: Recieved unexpected meesage of type %x\n",
                        reply->type);
                break;
            }

            react_timer(reply->timer_id);

            break;
        }
        case IPC_PS2_Send_Data_NUM: {
            IPC_PS2_Send_Data *d = (IPC_PS2_Send_Data *)message;
            react_send_data(d, desc.size);
            break;
        }
        default:
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x\n",
                    IPC_TYPE(message));
            break;
        }

        mutex_result = pthread_mutex_unlock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not unlock mutex\n");
            exit(mutex_result);
        }
        free(message);
    }

    return 0;
}