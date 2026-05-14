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
#include "pmbus.h"

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
#include <pmos/pmbus_helper.h>
#include <pmos/pmbus_object.h>

struct pmos_msgloop_data msgloop_data;

bool has_second_channel = false;

bool first_port_works  = false;
bool second_port_works = false;

bool enable_first_channel  = true;
bool enable_second_channel = true;

pmos_port_t main_port          = 0;
pmos_port_t configuration_port = 0;

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

pmos_right_t right_to_device = 0;

pmos_right_t int1_right = INVALID_RIGHT;
pmos_right_t int2_right = INVALID_RIGHT;

pmos_right_t get_interrupt_right(unsigned port)
{
    pmos_port_t control_port = get_control_port();
    if (control_port == 0) {
        printf("[i8042] Error: Could not get control port\n");
        exit(1);
    }

    IPC_Request_ACPI_Interrupt req = {
        .type  = IPC_Request_ACPI_Interrupt_NUM,
        .flags = 0,
        .index = port,
    };

    result_t result = send_message_right(right_to_device, control_port, (char *)&req, sizeof(req), NULL, 0).result;
    if (result != SUCCESS) {
        printf("[i8042] Error: Could not send message to get the interrupt\n");
        exit(1);
    }

    Message_Descriptor desc = {};
    unsigned char *message  = NULL;
    pmos_right_t rights[4];

    result = get_message(&desc, &message, control_port, NULL, rights);
    if (result != SUCCESS) {
        printf("[i8042] Error: Could not get message %i %s\n", (int)-result, strerror(-result));
        exit(1);
    }

    pmos_right_t int_right = INVALID_RIGHT;

    if (desc.size < sizeof(IPC_Request_Int_Reply)) {
        printf("[i8042] Warning: Recieved message which is too small (%i expected %i)\n",
               (int)desc.size, (int)sizeof(IPC_Request_Int_Reply));
        exit(1);
    }

    IPC_Request_Int_Reply *reply = (IPC_Request_Int_Reply *)message;
    if (reply->type != IPC_Request_Int_Reply_NUM) {
        printf("[i8042] Warning: Recieved unexepcted message type\n");
        exit(1);
    }

    if (reply->status == 0) {
        int_right = rights[0];
        rights[0] = INVALID_RIGHT;
    } else if (reply->status == -ENOENT) {
        // Don't do anything
    } else {
        printf("[i8042] Warning: Did not assign the interrupt, status: %i\n", (int)reply->status);
    }

// end:
    for (int i = 0; i < 4; i++) {
        if (rights[i] != INVALID_RIGHT)
            delete_right(rights[i]);
    }
    free(message);
    return int_right;
}

void *interrupt_thread(void *arg)
{
    ptrdiff_t port      = (ptrdiff_t)arg;
    ports_request_t req = create_port(TASK_ID_SELF, 0);
    if (req.result != SUCCESS) {
        printf("[i8042] Error creating port %" PRIi64 " port %i\n", req.result, (int)port);
        exit(1);
    }

    pmos_port_t int_port = req.port;
    pmos_right_t int_right = (port == 0) ? int1_right : int2_right;

    right_request_t r = set_interrupt(int_right, int_port);
    if (r.result != SUCCESS) {
        printf("[i8042] Error setting the interrupt %" PRIi64 " port %i\n", r.result, (int)port);
        exit(1);
    }
    pmos_right_t receive_right = r.right;

    while (1) {
        result_t result;

        Message_Descriptor desc = {};
        unsigned char *message  = NULL;
        result                  = get_message(&desc, &message, int_port, NULL, NULL);

        if (result != SUCCESS) {
            fprintf(stderr, "[i8042] Error: Could not get message %i %s\n", (int)-result,
                    strerror(-result));
        }

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "[i8042] Error: Message too small (size %" PRIu64 ")\n", desc.size);
            free(message);
        }

        if (IPC_TYPE(message) != IPC_Kernel_Interrupt_NUM) {
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x in the interrupt thread!\n",
                    IPC_TYPE(message));
            free(message);
            continue;
        }

        int mutex_result = pthread_mutex_lock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
            free(message);
            complete_interrupt(int_port, receive_right);
            continue;
        }

        if (IPC_TYPE(message) != IPC_Kernel_Interrupt_NUM) {
            fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x in the interrupt thread!\n",
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
            complete_interrupt(int_port, receive_right);
            exit(mutex_result);
        }
        complete_interrupt(int_port, receive_right);
        free(message);
    }
}

pthread_t thread1, thread2;

void init_interrupts()
{
    int1_right = get_interrupt_right(0);
    if (int1_right == INVALID_RIGHT) {
        printf("[i8042] Warning: Could not get interrupt right for port 1\n");
    } else {
        printf("[i8042] Info: Got interrupt right for port 1\n");
    }

    int2_right = get_interrupt_right(1);
    if (int2_right != INVALID_RIGHT) {
        printf("[i8042] Info: Got interrupt right for port 2\n");
    }

    if (int1_right != INVALID_RIGHT) {
        int result = pthread_create(&thread1, NULL, interrupt_thread, (void *)0);
        if (result < 0)
            printf("[i8042] Could not create thread for port 1: %i\n", result);
        else
            pthread_detach(thread1);
    }

    if (int2_right != INVALID_RIGHT) {
        int result = pthread_create(&thread2, NULL, interrupt_thread, (void *)1);
        if (result < 0)
            printf("[i8042] Could not create thread for port 2: %i\n", result);
        else
            pthread_detach(thread2);
    }
}

#define BRED "\e[1;31m"
#define BGRN "\e[1;32m"
#define CRESET "\e[0m"


struct pmbus_helper *pmbus_helper = NULL;

pmos_right_t port_0_right = 0;
pmos_right_t port_1_right = 0;

pmos_right_t port_0_receive_right = 0;
pmos_right_t port_1_receive_right = 0;

pmos_msgloop_tree_node_t port0_node, port1_node;

void publish_callback(int status, uint64_t object_id, void *ctx, struct pmbus_helper *helper)
{
    (void)helper;
    unsigned id = (unsigned)(uintptr_t)ctx;
    if (status < 0) {
        fprintf(stderr, "Failed to publish pmbus object for port %u: %i\n", id, status);
    } else {
        printf("i8042: Published object with id %" PRIu64 " for port %i!\n", object_id, id);
    }
}

int port_callback(Message_Descriptor *desc, void *msg,
                                       pmos_right_t *reply_right, pmos_right_t *other_rights,
                                       void *ctx, struct pmos_msgloop_data *)
{
    unsigned port = (int)(uintptr_t)ctx;

    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "i8042 recieved message too small for port %u!\n", port);
        return PMOS_MSGLOOP_CONTINUE;
    }

    uint32_t type = ((IPC_Generic_Msg *)msg)->type;

    switch (type) {
    case IPC_PS2_Reg_Port_NUM: {
        if (desc->size < sizeof(IPC_PS2_Reg_Port)) {
            fprintf(stderr, "i8042 recieved IPC_PS2_Reg_Port which is too small (size %u)\n", (unsigned)desc->size);
            break;
        }

        int mutex_result = pthread_mutex_lock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
            return PMOS_MSGLOOP_CONTINUE;
        }

        react_register_port(port, msg, reply_right, other_rights);

        mutex_result = pthread_mutex_unlock(&ports_mutex);
        if (mutex_result != 0) {
            fprintf(stderr, "[i8042] Error: Could not unlock mutex\n");
            exit(mutex_result);
        }

        break;
    }
    default:
        fprintf(stderr, "i8042 recieved message with unknown type for port %" PRIu32 "\n", type);
        break;
    }
    return PMOS_MSGLOOP_CONTINUE;
}

void register_port(unsigned char id)
{
    printf("i8042: registeting port %u\n", id);
    if (!pmbus_helper) {
        pmbus_helper = pmbus_helper_create(&msgloop_data);
        
        if (!pmbus_helper) {
            fprintf(stderr, "Failed to create pmbus helper (probably no memory!)\n");
            exit(1);
        }
    }

    pmos_right_t *right, *receive_right;
    pmos_msgloop_tree_node_t *node;
    if (id == 0) {
        right = &port_0_right;
        receive_right = &port_0_receive_right;
        node = &port0_node;
    } else {
        right = &port_1_right;
        receive_right = &port_1_receive_right;
        node = &port1_node;
    }

    right_request_t req = create_right(main_port, receive_right, 0);
    if (req.result) {
        fprintf(stderr, "Failed to create right for port %u: %i\n", id, (int)req.result);
        exit(1);
    }
    *right = req.right;

    pmos_msgloop_node_set(node, *receive_right, port_callback, (void *)(uintptr_t)id);
    pmos_msgloop_insert(&msgloop_data, node);

    pmos_bus_object_t *object = pmos_bus_object_create();
    
    char name[64];
    sprintf(name, "port_%i", id);

    if (!pmos_bus_object_set_name(object, name)) {
        fprintf(stderr, "Failed to set pmbus object name\n");
        exit(1);
    }

    if (!pmos_bus_object_set_property_string(object, "device", "ps2_port")) {
        fprintf(stderr, "Failed to set pmbus object property");
        exit(1);
    }

    if (!pmos_bus_object_set_property_integer(object, "i8042_port", id)) {
        fprintf(stderr, "Failed to set pmbus object property");
        exit(1);
    }

    req = dup_right(*right);
    if (req.result) {
        fprintf(stderr, "Failed to dup right for port %u: %i\n", id, (int)req.result);
        exit(1);
    }

    int result = pmbus_helper_publish(pmbus_helper, object, req.right, publish_callback, (void *)(uintptr_t)id);
    if (result < 0) {
        fprintf(stderr, "Failed to publish object for port %u: %i\n", id, result);
        exit(1);
    }
};

void register_ports()
{
    if (first_port_works)
        register_port(0);

    if (second_port_works)
        register_port(1);
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
        write_wait(RW_PORT, DISABLE_FIRST_PORT);

    if (second_port_works)
        write_wait(RW_PORT, DISABLE_SECOND_PORT);

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

void usage(char *name)
{
    printf("i8042 Usage: %s --right-id <ID of the right to the i8042 controller>\n", name);
    exit(1);
}

void parse_args(int argc, char **argv)
{
    if (argc < 1)
        printf("i8042: empty argc!\n");
    char *name = argv[0];

    bool have_right = false;

    for (int i = 1; i < argc; ++i) {
        char *r = argv[i];

        if (!strcmp(r, "--right-id")) {
            if (have_right) {
                printf("i8042: Repeated --right-id\n");
                usage(name);
            }

            ++i;
            if (i >= argc) {
                printf("i8042 missing right ID\n");
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
        printf("i8042: no right_id!\n");
        usage(name);
    }
}

int main_callback(Message_Descriptor *desc, void *message,
                                       pmos_right_t *, pmos_right_t *,
                                       void *, struct pmos_msgloop_data *)
{
    if (desc->size < IPC_MIN_SIZE) {
        fprintf(stderr, "[i8042] Error: Message too small (size %" PRIu64 ")\n", desc->size);
        return PMOS_MSGLOOP_CONTINUE;
    }

    int mutex_result = pthread_mutex_lock(&ports_mutex);
    if (mutex_result != 0) {
        fprintf(stderr, "[i8042] Error: Could not lock mutex\n");
        return PMOS_MSGLOOP_CONTINUE;
    }

    switch (IPC_TYPE(message)) {
    // case IPC_PS2_Send_Data_NUM: {
    //     IPC_PS2_Send_Data *d = (IPC_PS2_Send_Data *)message;
    //     react_send_data(d, desc.size);
    //     break;
    // }
    default:
        fprintf(stderr, "[i8042] Warning: Recieved message of unknown type %x in default callback. Sent from right: %" PRIu64 "\n",
                IPC_TYPE(message), desc->sent_with_right);
        break;
    }

    mutex_result = pthread_mutex_unlock(&ports_mutex);
    if (mutex_result != 0) {
        fprintf(stderr, "[i8042] Error: Could not unlock mutex\n");
        exit(mutex_result);
    }

    return PMOS_MSGLOOP_CONTINUE;
}

int main(int argc, char **argv)
{
    sleep(5);

    parse_args(argc, argv);
    printf("i8042 started. Right: %" PRIu64 "\n", right_to_device);

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
    }

    pmos_msgloop_initialize(&msgloop_data, main_port);
    request_ps2_rights();

    pmos_request_io_permission();
    //request_priority(1);
    init_controller();

    if (!first_port_works && !second_port_works) {
        fprintf(stderr, "[i8042] Error: No PS/2 ports apear to be useful...\n");
        exit(1);
    }

    register_ports();

    init_interrupts();

    enable_ports();

    pmos_msgloop_tree_node_t n;
    pmos_msgloop_node_set(&n, 0, main_callback, &msgloop_data);
    pmos_msgloop_insert(&msgloop_data, &n);
    pmos_msgloop_loop(&msgloop_data);

    return 0;
}