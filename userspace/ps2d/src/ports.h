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
#include <pmos/helpers.h>
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

extern enum Port_States state;
extern enum Device_Types type;
extern uint32_t device_id;
extern unsigned device_id_size;

extern uint64_t last_timer;
extern pmos_right_t port_send_right;

extern pmos_port_t main_port;

extern void (*managed_react_data)(unsigned char data);
extern void (*managed_react_timer)();

int send_data_port(const unsigned char *data, size_t data_size);
int send_byte_port(unsigned char byte);
void react_data(const char *data, size_t data_size);
void react_byte(unsigned char byte);
void start_port();
void reset_port();

#endif