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

#ifndef PORTS_H
#define PORTS_H
#include "keyboard.h"

#include <pmos/ipc.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum Port_States {
    PORT_STATE_RESET,
    PORT_STATE_DISABLE_SCANNING,
    PORT_STATE_IDENTIFY,
    PORT_STATE_WAIT,
    PORT_STATE_MANAGED,
};

enum Device_Types {
    DEVICE_TYPE_UNKNOWN,
    DEVICE_TYPE_KEYBOARD,
    DEVICE_TYPE_MOUSE,
};

// TODO: Replace with a better data structure

struct port_list_node {
    struct port_list_node *previous;
    struct port_list_node *next;

    // Owner of the port and its id
    uint64_t owner_pid;
    uint64_t port_id;

    // Port used for communications with the driver
    uint64_t com_port;

    uint64_t last_timer;

    uint32_t device_id;
    unsigned char device_id_size;

    enum Port_States state;
    enum Device_Types type;

    void (*managed_react_data)(struct port_list_node *port, unsigned char data);
    void (*managed_react_timer)(struct port_list_node *port);

    union {
        struct keyboard_state kb_state;
    };
};

extern struct port_list_node *first;
extern struct port_list_node *last;

void list_push_back(struct port_list_node *n);

// Takes out the node out of the list. Does not free it.
void list_take_out(struct port_list_node *n);

// Get the node with the given owner_id and port_if. Return NULL if no node was found
struct port_list_node *list_get(uint64_t owner_pid, uint64_t port_id);

bool register_port(IPC_PS2_Reg_Port *message, uint64_t sender);
void react_message(IPC_PS2_Notify_Data *message, uint64_t sender, uint64_t size);

void send_data_port(struct port_list_node *port, const unsigned char *data, size_t data_size);
void send_byte_port(struct port_list_node *port, unsigned char byte);
void react_data(struct port_list_node *port, const char *data, size_t data_size);
void react_byte(struct port_list_node *port, unsigned char byte);
void start_port(struct port_list_node *port);
uint64_t port_start_timer(struct port_list_node *port, unsigned time_ms);
void react_timer(IPC_Timer_Reply *tmr);
void port_react_timer(struct port_list_node *port, uint64_t timer_id);
void reset_port(struct port_list_node *port);

#endif