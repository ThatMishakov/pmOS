#include <stdint.h>
#include <acpi/acpi.h>
#include <pmos/memory.h>
#include <serial.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct serial_port * serial_port = NULL;

void init_serial() {
    struct serial_port * port = NULL;

    printf("Searching for serial devices...\n");

    SPCR * t = (SPCR *)get_table("SPCR", 0);
    if (!t) {
        printf("SPCR table not found\n");
        return;
    }

    printf("Found SPCR table\n");

    port = malloc(sizeof *port);
    if (!port) {
        fprintf(stderr, "Failed to allocate memory for serial port\n");
        return;
    }

    port->access_type = t->address.AddressSpace;
    
    uint8_t access_width = t->address.AccessSize;
    if (access_width < 1 || access_width > 4) {
        fprintf(stderr, "Warning: SPCR Invalid access width %i\n", access_width);
        port->access_width = 8;
    } else {
        port->access_width = 1 << (access_width + 2);
    
    }

    port->interface_type = t->interface_type;
    port->base_address = t->address.Address;

    port->interrupt = t->interrupt_type;
    port->pc_intno = t->pc_irq;
    port->gsi = t->interrupt;
    
    switch (t->configured_baud_rate) {
        case 0:
            port->baud_rate = 0;
            break;
        case 3:
            port->baud_rate = 9600;
            break;
        case 4:
            port->baud_rate = 19200;
            break;
        case 5:
            port->baud_rate = 38400;
            break;
        case 6:
            port->baud_rate = 57600;
            break;
        case 7:
            port->baud_rate = 115200;
            break;
        default:
            fprintf(stderr, "SPCR Invalid baud rate %i\n", t->configured_baud_rate);
            goto error;
    }

    port->parity = t->parity;
    port->stop_bits = t->stop_bits;	
    port->flow_control = t->flow_control;
    port->terminal_type = t->terminal_type;

    serial_port = port;

    printf("Serial port found:\n");
    printf("ACPI version: %i\n", t->h.revision);
    printf("Access type: %s\n", port->access_type == 0 ? "mmio" : "io");
    printf("Access width: %i\n", port->access_width);
    printf("Interface type: %i\n", port->interface_type);
    printf("Base address: %lx\n", port->base_address);
    printf("Interrupt type: %i\n", port->interrupt);
    printf("PC-AT interrupt number: %i\n", port->pc_intno);
    printf("Global System Interrupt: %i\n", port->gsi);
    printf("Baud rate: %i\n", port->baud_rate);
    printf("Parity: %i\n", port->parity);
    printf("Stop bits: %i\n", port->stop_bits);
    printf("Flow control: %i\n", port->flow_control);
    printf("Terminal type: %i\n", port->terminal_type);

    return;

error:
    free(port);
    return;
}

void request_serial(Message_Descriptor *d, IPC_Request_Serial *m)
{
    IPC_Serial_Reply reply;
    result_t result;

    if (d->size < sizeof(IPC_Request_Serial)) {
        // TODO: No way to determine the reply channel
        fprintf(stderr, "Invalid message size %li from task %li\n", d->size, d->sender);
        return;
    }

    if (!serial_port) {
        reply = (IPC_Serial_Reply) {
            .type = IPC_Serial_Reply_NUM,
            .result = -ENODEV,
        };

        result = send_message_port(m->reply_port, sizeof(reply), (char*)&reply);
        if (result != SUCCESS) {
            fprintf(stderr, "Failed to send message to task %li: %lx\n", d->sender, result);
        }
        return;
    }

    reply = (IPC_Serial_Reply) {
        .type = IPC_Serial_Reply_NUM,
        .result = 0,
        .access_type = serial_port->access_type,
        .access_width = serial_port->access_width,
        .interface_type = serial_port->interface_type,
        .base_address = serial_port->base_address,
        .interrupt_type = serial_port->interrupt,
        .pc_int_number = serial_port->pc_intno,
        .gsi_number = serial_port->gsi,
        .baud_rate = serial_port->baud_rate,
        .parity = serial_port->parity,
        .stop_bits = serial_port->stop_bits,
        .flow_control = serial_port->flow_control,
        .terminal_type = serial_port->terminal_type,
    };

    result = send_message_port(m->reply_port, sizeof(reply), (char*)&reply);
    if (result != SUCCESS) {
        fprintf(stderr, "Failed to send message to task %li: %lx\n", d->sender, result);
    }
}