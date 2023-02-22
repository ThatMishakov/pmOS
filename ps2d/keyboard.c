#include "keyboard.h"
#include "ports.h"
#include "commands.h"
#include <stdio.h>

unsigned keyboard_ids[] = {
    0x83ab,
};

bool is_keyboard(struct port_list_node *port)
{
    if (port->device_id_size == 0)
        return true;

    if (port->device_id_size != 2)
        return false;

    for (unsigned i = 0; i < sizeof(keyboard_ids)/sizeof(keyboard_ids[0]); ++i)
        if (port->device_id == keyboard_ids[i])
            return true;
    
    return false;
}



void keyboard_react_data(struct port_list_node *port, unsigned char data)
{
    port->kb_state.send_status = STATUS_RECIEVED_DATA;

    switch (data) {
    case RESPONSE_ECHO:
    case RESPONSE_ACK: // Acked command
        keyboard_ack_cmd(port);
        break;

    case RESPONSE_RESEND:
        keyboard_send_front_cmd(port);
        break;

    case RESPONSE_SELF_TEST_OK: // New device connected
        unregister_keyboard(port);
        reset_port(port);
        break;

    default:
        keyboard_scan_byte(port, data);
        break;
    }
}

void keyboard_react_timer(struct port_list_node *port)
{
    switch (port->kb_state.send_status)
    {
    case STATUS_NODATA:
        port->kb_state.send_status = STATUS_SENT_ECHO;
        keyboard_push_cmd_byte(port, COMMAND_ECHO);
        port_start_timer(port, 1000);
        break;
    
    case STATUS_RECIEVED_DATA:
        port->kb_state.send_status = STATUS_NODATA;
        port_start_timer(port, 1000);
        break;

    case STATUS_SENT_ECHO:
        fprintf(stderr, "[PS2d] Warning: Keyboard at port %lx PID %lx did not ACK STATUS_SENT_ECHO command\n", port->port_id, port->owner_pid);
        unregister_keyboard(port);

        reset_port(port);
        break;
    }
}


bool init_keyboard(struct port_list_node *port)
{
    printf("[PS2d] Info: Found keyboard on port %lx PID %lx\n", port->port_id, port->owner_pid);

    register_keyboard(port);

    port->managed_react_data = &keyboard_react_data;
    port->managed_react_timer = &keyboard_react_timer;
    port->state = PORT_STATE_MANAGED;
    port->kb_state.send_status = STATUS_NODATA;
    port->kb_state.cmd_buffer_index = 0;
    port->kb_state.cmd_buffer_start = 0;

    port->kb_state.scancode = 0;
    port->kb_state.expected_bytes = 1;
    port->kb_state.shift_val = 0;

    keyboard_push_cmd_byte(port, COMMAND_ENABLE_SCANNING);
    port_start_timer(port, 1000);

    return true;
}

void register_keyboard(struct port_list_node *port)
{

}

void unregister_keyboard(struct port_list_node *port)
{

}

void keyboard_send_front_cmd(struct port_list_node *port)
{
    struct keyboard_cmd cmd = keyboard_cmd_get_front(port);

    send_data_port(port, cmd.cmd, cmd.size);
}

void keyboard_push_cmd_byte(struct port_list_node *port, unsigned char data)
{
    struct keyboard_cmd cmd = {
        .cmd = {
            [0] = data,
            [1] = 0x00,
        },
        .complex_response = false,
        .size = 1,
    };

    keyboard_push_cmd(port, cmd);
}

void keyboard_ack_cmd(struct port_list_node *port)
{
    if (!keyboard_cmd_queue_empty(port))
        keyboard_pop_front_cmd(port);

    if (!keyboard_cmd_queue_empty(port))
        keyboard_send_front_cmd(port);   
}

bool keyboard_cmd_queue_empty(struct port_list_node *port)
{
    return port->kb_state.cmd_buffer_index == port->kb_state.cmd_buffer_start;
}

struct keyboard_cmd keyboard_cmd_get_front(struct port_list_node *port)
{
    return port->kb_state.cmd_buffer[port->kb_state.cmd_buffer_index];
}

void keyboard_push_cmd(struct port_list_node *port, struct keyboard_cmd data)
{
    unsigned new_keyboard_index = port->kb_state.cmd_buffer_start + 1;
    if (new_keyboard_index == KBD_CMD_BUFF_SIZE)
        new_keyboard_index = 0;

    if (new_keyboard_index == port->kb_state.cmd_buffer_index) {
        fprintf(stderr, "[PS2d] Warning: Keyboard at port %li PID %li hat its buffer full. Discarding the last command...\n", port->port_id, port->owner_pid);
    } else {
        bool empty_before = keyboard_cmd_queue_empty(port);

        port->kb_state.cmd_buffer[port->kb_state.cmd_buffer_start] = data;
        port->kb_state.cmd_buffer_start = new_keyboard_index;

        if (empty_before)
            keyboard_send_front_cmd(port);
    }

}

void keyboard_pop_front_cmd(struct port_list_node *port)
{
    ++port->kb_state.cmd_buffer_index;
    if (port->kb_state.cmd_buffer_index == KBD_CMD_BUFF_SIZE)
        port->kb_state.cmd_buffer_index = 0;
}

void keyboard_scan_byte(struct port_list_node *port, unsigned char data)
{
    port->kb_state.scancode |= (uint64_t)data << port->kb_state.shift_val;

    port->kb_state.shift_val += 8;

    switch (data)
    {
    case 0xF0: // break
    case 0xE0: // extended
        // Do not decrement expected bytes
        break;
    
    default:
        port->kb_state.expected_bytes -= 1;
        break;
    }

    if (port->kb_state.expected_bytes == 0) {
        printf("Keyboard scancode %lx\n", port->kb_state.scancode);
        port->kb_state.scancode = 0;
        port->kb_state.shift_val = 0;
        port->kb_state.expected_bytes = 1;
    }
}