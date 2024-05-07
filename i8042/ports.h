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
#include <pmos/ipc.h>
#include <pmos/ports.h>
#include <stdbool.h>
#include <stdint.h>

extern bool first_port_works;
extern bool second_port_works;

typedef enum Port_States {
    PORT_STATE_RESET,
    PORT_STATE_OK,
    PORT_STATE_OK_NOINPUT,
    PORT_STATE_IDENTIFY,
    PORT_STATE_DISABLE_SCANNING,
    PORT_STATE_ENABLE_SCANNING,
    PORT_STATE_WAIT,
} Port_States;

extern int port1_state;
extern int port2_state;

void react_port1_int();
void react_port2_int();
void react_timer(uint64_t index);

void init_ports();
void poll_ports();

void react_send_data(IPC_PS2_Send_Data *str, size_t message_size);

typedef struct Port {
    pmos_port_t notification_port;
} Port;

extern uint64_t last_polling_timer;

extern Port ports[2];

static const uint32_t alive_interval = 2000;

#endif