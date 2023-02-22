#ifndef KEYBOARD_H
#define KEYBOARD_H
#include <stdbool.h>
#include <stdint.h>

struct port_list_node;

bool is_keyboard(struct port_list_node *port);
bool init_keyboard(struct port_list_node *port);

void keyboard_react_timer(struct port_list_node *port);
void keyboard_react_data(struct port_list_node *port, unsigned char data);

void keyboard_scan_byte(struct port_list_node *port, unsigned char data);

// Registers keyboard with HID daemon
void register_keyboard(struct port_list_node *port);
void unregister_keyboard(struct port_list_node *port);

#define KBD_CMD_BUFF_SIZE 16

struct keyboard_cmd {
    unsigned char cmd[2];
    unsigned char size;
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

    struct keyboard_cmd cmd_buffer[KBD_CMD_BUFF_SIZE];
    unsigned cmd_buffer_start;
    unsigned cmd_buffer_index;
};

struct keyboard_cmd keyboard_cmd_get_front(struct port_list_node *port);
void keyboard_push_cmd(struct port_list_node *port, struct keyboard_cmd data);
void keyboard_push_cmd_byte(struct port_list_node *port, unsigned char data);
void keyboard_ack_cmd(struct port_list_node *port);
void keyboard_pop_front_cmd(struct port_list_node *port);
void keyboard_send_front_cmd(struct port_list_node *port);
bool keyboard_cmd_queue_empty(struct port_list_node *port);

void keyboard_react_scancode(struct port_list_node *port, uint64_t scancode);

#endif