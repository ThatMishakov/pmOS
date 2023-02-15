#include "ports.h"
#include "io.h"
#include "registers.h"
#include <stdio.h>
#include "timers.h"

Port ports[2];
uint64_t last_polling_timer = 0;

void send_data_port(uint8_t cmd, bool port_2)
{
    uint8_t status;

    if (port_2) {
        outb(RW_PORT, SECOND_PORT);
    }

    outb(DATA_PORT, cmd);
}

void init_ports()
{
    ports[0].state = PORT_STATE_RESET;
    ports[0].last_timer = 0;

    ports[1].state = PORT_STATE_RESET;
    ports[1].last_timer = 0;

    if (first_port_works) {
        send_data_port(0xff, false);
    }

    if (second_port_works) {
        if (first_port_works) {
            ports[1].state = PORT_STATE_WAIT;
            ports[1].last_timer = start_timer(500);
        }
    }

    poll_ports();
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


void react_data(uint8_t data, char port_num)
{
    Port* port = &ports[port_num];
    // printf("--- INT port %i data %x state %i\n", port_num, data, port->state);

    switch (port->state) {
    case PORT_STATE_RESET:
        switch (data) {
        case RESPONSE_FAILURE:

            printf("Port %i failure!\n", port_num+1);
            break;
        case RESPONSE_ACK:
            #ifdef DEBUG
            printf("Port %i ACK\n", port_num+1);
            #endif
            break;
        case RESPONSE_SELF_TEST_OK:
            #ifdef DEBUG
            printf("Port %i self-test success!\n", port_num+1);
            #endif
            port->state = PORT_STATE_DISABLE_SCANNING;

            port->device_id = 0;
            port->alive = false;

            send_data_port(COMMAND_DISABLE_SCANNING, port_num);

            port->last_timer = start_timer(200);
        default:
            // printf("%i\n", data);
            break;
        }
        break;


    case PORT_STATE_DISABLE_SCANNING:
    switch (data)
    {
    case RESPONSE_ACK:
        port->state = PORT_STATE_IDENTIFY;

        #ifdef DEBUG
        printf("Port %i disabled scanning successfully!\n",port_num+1);
        #endif

        send_data_port(COMMAND_IDENTIFY, port_num);

        port->last_timer = start_timer(700);
        break;
    default:
        // Ignore data
        break;
    }
    break;


    case PORT_STATE_IDENTIFY:
        if (data == RESPONSE_ACK)
            break;
        port->device_id <<= 8;
        port->device_id |= data;
    break;


    case PORT_STATE_ENABLE_SCANNING:
    switch (data)
    {
    case RESPONSE_ACK:
        port->state = PORT_STATE_OK;
        port->alive = false;

        port->last_timer = start_timer(alive_interval);

        #ifdef DEBUG
        printf("Port %i enabled scanning successfully!\n",port_num+1);
        #endif

        break;
    default:
        // Ignore
        break;
    }
    break;


    case PORT_STATE_OK_NOINPUT:
        port->state = PORT_STATE_OK;
        port->alive = true;

        if (data == RESPONSE_ECHO)
            break;

        __attribute__((fallthrough));
    case PORT_STATE_OK: {
        printf("Port %i data %x\n",port_num+1, data);
        port->alive = true;
        break;
    }
    break;
    };
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

void react_timer_port(unsigned port_num)
{
    Port* port = &ports[port_num];

    switch (port->state) {
    case PORT_STATE_RESET:
        // Do nothing
        break;
    case PORT_STATE_ENABLE_SCANNING:
    case PORT_STATE_DISABLE_SCANNING:
    case PORT_STATE_OK_NOINPUT:
        printf("Warning: PS/2 port %x timeout\n", port_num+1);
        port->state = PORT_STATE_WAIT;
        port->last_timer = start_timer(5000);
        break;
    case PORT_STATE_OK:
        if (port->alive) {
            port->alive = false;
        } else {
            if (inb(RW_PORT) & STATUS_MASK_INPUT_FULL) {
            }

            port->state = PORT_STATE_OK_NOINPUT;
            send_data_port(COMMAND_ECHO, port_num);
        }
        port->last_timer = start_timer(alive_interval);
        break;
    case PORT_STATE_IDENTIFY: {
        printf("Info: Found PS/2 device on port %i with type 0x%X\n", port_num+1, port->device_id);
        bool result_success = true;

        if (result_success) {
            port->state = PORT_STATE_ENABLE_SCANNING;

            send_data_port(COMMAND_ENABLE_SCANNING, port_num);

            port->last_timer = start_timer(800);
        } else {
            port->state = PORT_STATE_RESET;
        }
        break;
    case PORT_STATE_WAIT:
        port->state = PORT_STATE_RESET;
        send_data_port(COMMAND_RESET, port_num);
    }

    }
}

void poll_ports()
{
    char data = 0;
    bool second = second_port_works;

    bool two_ports_work = first_port_works && second_port_works;

    bool* check_second = two_ports_work ? &second : NULL;
    bool have_read_data = read_data(&data, check_second);

    last_polling_timer = start_timer(200);

    if (!have_read_data)
        return;

    react_data(data, second);
}

void react_timer(uint64_t index)
{
    if (ports[0].last_timer == index) {
        react_timer_port(0);
    } else if (ports[1].last_timer == index) {
        react_timer_port(1);
    } else if (last_polling_timer == index) {
        poll_ports();
    }
}