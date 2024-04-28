#include <stdint.h>
#include <acpi/acpi.h>
#include <pmos/memory.h>
#include <serial.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>
#include <smoldtb.h>

struct serial_port * serial_port = NULL;

bool init_serial_acpi()
{
    struct serial_port * port = NULL;

    SPCR * t = (SPCR *)get_table("SPCR", 0);
    if (!t) {
        printf("SPCR table not found\n");
        goto error;
    }

    printf("Found SPCR table\n");

    port = malloc(sizeof *port);
    if (!port) {
        fprintf(stderr, "Failed to allocate memory for serial port\n");
        goto error;
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
            fprintf(stderr, "SPCR Invalid baud rate %i. Setting 115200\n", t->configured_baud_rate);
            port->baud_rate = 115200;
    }

    printf("Serial port found. ACPI version: %i\n", t->h.revision);

    port->parity = t->parity;
    port->stop_bits = t->stop_bits;	
    port->flow_control = t->flow_control;
    port->terminal_type = t->terminal_type;

    serial_port = port;

    return true;

error:
    free(port);
    return false;
}

bool init_serial_dtb()
{
    struct serial_port * port = NULL;

    const char *serial_compatible_list[] = {
        "ns16550a"
    };

    dtb_node *n = NULL;
    for (size_t i = 0; (i < sizeof(serial_compatible_list)/sizeof(serial_compatible_list[0])) && (n == NULL); ++i) {
        n = dtb_find_compatible(NULL, serial_compatible_list[i]);
    }

    if (n == NULL) {
        fprintf(stderr, "Could not find a compatible ns16550a serial controller in FDT\n");
        goto error;
    }

    // Find the registers size
    dtb_node *soc = dtb_get_parent(n);
    if (!soc) {
        fprintf(stderr, "Could not find the parent DTB node of the serial controller\n");
        goto error;
    }

    size_t address_cells = 0;
    dtb_prop *prop = dtb_find_prop(soc, "#address-cells");
    size_t i = dtb_read_prop_values(prop, 1, &address_cells);
    if (i != 1) {
        fprintf(stderr, "Could not read the #address-cells property in the parent DTB node\n");
        goto error;
    }

    size_t size_cells = 0;
    prop = dtb_find_prop(soc, "#size-cells");
    i = dtb_read_prop_values(prop, 1, &size_cells);
    if (i != 1) {
        fprintf(stderr, "Could not read the #size-cells property in the parent DTB node\n");
        goto error;
    }


    // Find the address and size of the registers
    prop = dtb_find_prop(n, "reg");
    dtb_pair reg;
    i = dtb_read_prop_pairs(prop, (dtb_pair){address_cells, size_cells}, &reg);
    if (i != 1) {
        fprintf(stderr, "Could not read the reg property in the serial controller node\n");
        goto error;
    }

    size_t serial_address = reg.a;
    size_t serial_size = reg.b;
    (void)serial_size; // Unused

    // Find the interrupt property
    size_t interrupt = 0;
    prop = dtb_find_prop(n, "interrupts");
    i = dtb_read_prop_values(prop, 1, &interrupt);
    if (i != 1) {
        fprintf(stderr, "Could not read the interrupts property in the serial controller node\n");
    }

    size_t current_speed = 0;
    prop = dtb_find_prop(n, "current-speed");
    // current-speed is optional
    dtb_read_prop_values(prop, 1, &current_speed);

    port = calloc(1, sizeof *port);

    port->interface_type = 0;
    port->base_address = serial_address;

    port->gsi = interrupt;
    #ifdef __riscv
    port->interrupt = port->gsi == 0 ? 0x0 : 0x10;
    #endif 
    port->interface_type = 0;
    port->pc_intno = 0;
    port->baud_rate = current_speed;
    port->parity = 0;
    port->stop_bits = 1;
    // TODO: Flow control
    port->flow_control = 0;
    port->terminal_type = 0;
    port->access_type = 0;
    port->access_width = 1;

    serial_port = port;

    printf("Serial port found in FDT\n");

    return true;
error:
    free(port);
    return false;
}

bool init_serial_hardcoded()
{
    printf("Using hardcoded serial port values\n");

    // Hardcoded values!
    struct serial_port * port = malloc(sizeof *port);
    port->interface_type = 0;
    port->base_address = 0x10000000;
    port->interrupt = 0x10;
    port->pc_intno = 0;
    port->gsi = 10;
    port->baud_rate = 115200;
    port->parity = 0;
    port->stop_bits = 1;
    port->flow_control = 0;
    port->terminal_type = 0;
    port->access_type = 0;
    port->access_width = 1;

    serial_port = port;

    return true;
}

void init_serial()
{
    printf("Searching for serial devices...\n");

    do {
        if (init_serial_acpi()) {
            break;
        }

        if (init_serial_dtb()) {
            break;
        }

        if (init_serial_hardcoded()) {
            break;
        }

        fprintf(stderr, "No serial port found\n");
        return;
    } while (0);

    printf("Access type: %s\n", serial_port->access_type == 0 ? "mmio" : "io");
    printf("Access width: %i\n", serial_port->access_width);
    printf("Interface type: %i\n", serial_port->interface_type);
    printf("Base address: %lx\n", serial_port->base_address);
    printf("Interrupt type: %i\n", serial_port->interrupt);
    printf("PC-AT interrupt number: %i\n", serial_port->pc_intno);
    printf("Global System Interrupt: %i\n", serial_port->gsi);
    printf("Baud rate: %i\n", serial_port->baud_rate);
    printf("Parity: %i\n", serial_port->parity);
    printf("Stop bits: %i\n", serial_port->stop_bits);
    printf("Flow control: %i\n", serial_port->flow_control);
    printf("Terminal type: %i\n", serial_port->terminal_type);
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