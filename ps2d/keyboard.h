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

#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

struct port_list_node;

bool is_keyboard();
bool init_keyboard();

void keyboard_react_timer();
void keyboard_react_data(unsigned char data);

void keyboard_scan_byte(unsigned char data);

// Registers keyboard with HID daemon
void register_keyboard();
void unregister_keyboard();

#define KBD_CMD_BUFF_SIZE 16

struct keyboard_cmd {
    unsigned char cmd[2];
    unsigned char size: 4;
    unsigned char acked_count: 4;
    bool complex_response;
};

struct keyboard_state {
    uint64_t scancode;
    unsigned expected_bytes;
    unsigned shift_val;

    enum {
        STATUS_RECIEVED_DATA,
        STATUS_NODATA,
        STATUS_SENT_ECHO,
    } send_status;

    enum {
        STATUS_SCAN_CODE_REQUEST,
    } operation_status;

    struct keyboard_cmd cmd_buffer[KBD_CMD_BUFF_SIZE];
    unsigned cmd_buffer_start;
    unsigned cmd_buffer_index;
};

struct keyboard_cmd *keyboard_cmd_get_front();
void keyboard_push_cmd(struct keyboard_cmd data);
void keyboard_push_cmd_byte(unsigned char data);
void keyboard_ack_cmd();
void keyboard_pop_front_cmd();
void keyboard_send_front_cmd();
bool keyboard_cmd_queue_empty();

void keyboard_react_scancode(uint64_t scancode);

#endif