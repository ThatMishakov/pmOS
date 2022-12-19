#include "ports.h"
#include "io.h"
#include "registers.h"
#include <stdio.h>

int port1_state = PORT_STATE_RESET;
int port2_state = PORT_STATE_RESET;

void react_port1_int()
{
    unsigned char data = inb(DATA_PORT);

    switch (port1_state) {
    case PORT_STATE_RESET:
        switch (data) {
        case RESPONSE_SELF_TEST_OK:
            printf("Port 1 success!\n");
            port1_state = PORT_STATE_OK;
            break;
        case RESPONSE_FAILURE:
            printf("Port 1 failure!\n");
            port1_state = PORT_STATE_FAILURE;
            break;
        case RESPONSE_ACK:
            printf("Port 1 ACK\n");
            break;
        }
        break;
    case PORT_STATE_OK: {
        printf("Port 1 data %x\n", data);
        break;
    }
    };
}

void react_port2_int()
{
    unsigned char data = inb(DATA_PORT);

    switch (port2_state) {

    default:
        printf("Port 2 state %i data %x\n", port1_state, data);
        break;
    };

}