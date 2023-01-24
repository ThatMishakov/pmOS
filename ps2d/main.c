#include <stdio.h>
#include "interrupts.h"
#include "io.h"
#include "registers.h"
#include <stdbool.h>
#include <stdlib.h>
#include <pmos/system.h>
#include <kernel/block.h>
#include <pmos/helpers.h>
#include "ports.h"

bool has_second_channel = false;

bool first_port_works =  false;
bool second_port_works = false;

void init_controller()
{
    uint8_t status;
    uint8_t data;

    // Disable ports
    outb(RW_PORT, DISABLE_FIRST_PORT);
    outb(RW_PORT, DISABLE_SECOND_PORT);

    // Clear output buffer
    data = inb(DATA_PORT); 

    // Configure the controller
    outb(RW_PORT, CMD_CONFIG_READ);
    data = inb(DATA_PORT);

    data &= ~0x43;
    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, data);

    if (data & 0x20) has_second_channel = true; 

    // Perform self-test
    outb(RW_PORT, TEST_CONTROLLER);
    do {
        status = inb(RW_PORT);
    } while (!(status & STATUS_MASK_OUTPUT_FULL));
    data = inb(DATA_PORT);
    if (data != 0x55) {
        printf("Error: PS/2 controller did not pass self-test (status %x)\n", data);
        exit(1);
    }

    // Test if the controller has second channel
    if (has_second_channel) {
        outb(RW_PORT, ENABLE_SECOND_PORT);
        data = inb(DATA_PORT);

        if (data & 0x05)
            has_second_channel = false;
        else {
            outb(RW_PORT, DISABLE_SECOND_PORT);
        }
    }

    outb(RW_PORT, TEST_FIRST_PORT);
    do {
        status = inb(RW_PORT);
    } while (!(status & STATUS_MASK_OUTPUT_FULL));
    data = inb(DATA_PORT);
    if (!data) {
        printf("Info: Port 1 passed self-test\n");
        first_port_works = true;
    } else {
        printf("Notice: first port didn't pass self-test (error %x)\n", data);
    }

    if (has_second_channel) {
        outb(RW_PORT, TEST_SECOND_PORT);
        do {
            status = inb(RW_PORT);
        } while (!(status & STATUS_MASK_OUTPUT_FULL));
        data = inb(DATA_PORT);
        if (!data) {
            printf("Info: Port 2 passed self-test\n");
            second_port_works = true;
        } else {
            printf("Notice: second port didn't pass self-test (error %x)\n", data);
        }
    }
}

void enable_ports()
{
    uint8_t data;
    outb(RW_PORT, CMD_CONFIG_READ);
    data = inb(DATA_PORT);

    if (first_port_works) data |= 0x01;
    if (second_port_works) data |= 0x02;

    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, data);

    if (first_port_works)
        outb(RW_PORT, ENABLE_FIRST_PORT);

    if (second_port_works)
        outb(RW_PORT, ENABLE_SECOND_PORT);
}

uint8_t port1_int = 0;
uint8_t port2_int = 0;

int main(int argc, char *argv[])
{
    printf("Hello from PS2d!\n");

    init_controller();

    if (!first_port_works && !second_port_works) {
        fprintf(stderr, "Error: No PS/2 ports apear to be useful...\n");
        exit(1);
    }

    if (first_port_works)
        port1_int = get_interrupt_number(1, int1_chan);

    if (second_port_works)
        port2_int = get_interrupt_number(12, int12_chan);

    enable_ports();

    outb(DATA_PORT, 0xff);

    while (1) {
        result_t result;
        syscall_r r = block(MESSAGE_UNBLOCK_MASK);
        if (r.result != SUCCESS) {
            printf("Warning: Could not block\n");
            return 0;
        }

        Message_Descriptor desc = {};
        unsigned char* message = NULL;
        result = get_message(&desc, &message);

        if (result != SUCCESS) {
            fprintf(stderr, "Error: Could not get message\n");
        }

        switch (desc.channel) {
        case int1_chan:
            react_port1_int();
            break;
        case int12_chan:
            react_port2_int();
            break;
        default:
            fprintf(stderr, "Warning: Recieved message on unknown channel %lx\n", desc.channel);
            break;
        }
    }

    return 0;
}