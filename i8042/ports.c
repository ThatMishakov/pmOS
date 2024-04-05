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

#include "ports.h"
#include "io.h"
#include "registers.h"
#include <stdio.h>
#include "timers.h"
#include <alloca.h>
#include <pmos/ipc.h>
#include <pmos/system.h>

Port ports[2];
uint64_t last_polling_timer = 0;

bool send_data_port(uint8_t cmd, bool port_2)
{
    unsigned count = 0;
    uint8_t status = 0;

    if (port_2) {
        do {
            status = inb(RW_PORT);
            if (count++ > 1000)
                return false;
        } while (status & STATUS_MASK_OUTPUT_FULL);

        outb(RW_PORT, SECOND_PORT);
    }

    status = 0;
    do {
        status = inb(RW_PORT);
        if (count++ > 1000)
            return false;
    } while (status & STATUS_MASK_OUTPUT_FULL);

    outb(DATA_PORT, cmd);
    return true;
}

bool read_data(char* data, bool* is_second)
{
    if (!data)
        return false;

    uint8_t status = inb(RW_PORT);
    if (!(status & STATUS_MASK_OUTPUT_FULL)) {
        // No data to read
        return false;
    }

    if (is_second) {
        *is_second = !!(status & STATUS_MASK_SECOND_FULL);
    }

    *data = inb(DATA_PORT);

    return true;
}


void react_data(uint8_t data, unsigned char port_num)
{
    size_t size = sizeof(IPC_PS2_Notify_Data) + 1;

    IPC_PS2_Notify_Data* str = alloca(size);

    str->type = IPC_PS2_Notify_Data_NUM;
    str->flags = 0;
    str->internal_id = port_num;
    str->data[0] = data;

    result_t result = send_message_port(ports[port_num].notification_port, size, (char *)str);
    if (result != SUCCESS) {
        printf("[i8042] Warning: Could not send message to get the interrupt\n");
    }
}

void react_port_int(unsigned port_num)
{
    char data = 0;
    bool second = port_num;

    bool two_ports_work = first_port_works && second_port_works;

    bool* check_second = two_ports_work ? &second : NULL;
    bool have_read_data = read_data(&data, check_second);

    if (!have_read_data)
        return;

    react_data(data, second);
}

void react_port1_int()
{
    react_port_int(0);
}

void react_port2_int()
{
    react_port_int(1);
}

void poll_ports()
{
    char data = 0;
    bool second = second_port_works;

    bool two_ports_work = first_port_works && second_port_works;

    bool* check_second = two_ports_work ? &second : NULL;
    bool have_read_data = read_data(&data, check_second);

    last_polling_timer = start_timer(100);

    if (!have_read_data)
        return;

    react_data(data, second);
}

void react_timer(uint64_t index)
{
    if (last_polling_timer == index) {
        poll_ports();
    } else {
        fprintf(stderr, "[i8042] Warning: Recieved timer message with unknown index %li\n", index);
    }
}

void react_send_data(IPC_PS2_Send_Data* str, size_t message_size)
{
    if (message_size < sizeof(IPC_PS2_Send_Data)) {
        fprintf(stderr, "[i8042] Warning: IPC_PS2_Send_Data with size %lx smaller than expected\n", message_size);
    }

    unsigned port = str->internal_id;
    size_t data_size = message_size - sizeof(IPC_PS2_Send_Data);

    for (size_t i = 0; i < data_size; ++i)
        send_data_port(str->data[i], port);
}