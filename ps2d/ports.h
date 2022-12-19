#ifndef PORTS_H
#define PORTS_H

#define PORT_STATE_RESET       0
#define PORT_STATE_NOT_PRESENT 1
#define PORT_STATE_FAILURE     2
#define PORT_STATE_OK          3

extern int port1_state;
extern int port2_state;

void react_port1_int();
void react_port2_int();

#endif