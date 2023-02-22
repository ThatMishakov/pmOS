#include "configuration.h"
#include "ports.h"
#include "stdio.h"
#include "commands.h"
#include "keyboard.h"

void unknown_react_timer(struct port_list_node *port)
{

}

void unknown_react_data(struct port_list_node *port, unsigned char data)
{
    if (data == RESPONSE_SELF_TEST_OK) {
        // Another device has been connected

        reset_port(port);
    }
}

void configure_port(struct port_list_node *port)
{
    bool inited_port = false;

    if (is_keyboard(port)) {
        inited_port = init_keyboard(port);
    }

    if (!inited_port) {
        printf("[PS2d] Info: Found unknown device on port %li PID %li with type 0x%x (size %i)\n", port->port_id, port->owner_pid, port->device_id, port->device_id_size);

        port->state = PORT_STATE_MANAGED;
        port->managed_react_data = &unknown_react_data;
        port->managed_react_timer = &unknown_react_timer;
    }
}
