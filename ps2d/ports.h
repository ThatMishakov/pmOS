#ifndef PORTS_H
#define PORTS_H
#include <stdint.h>
#include <stdbool.h>

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

static const uint32_t alive_interval = 2000;

#endif