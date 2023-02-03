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
#include <pmos/ipc.h>
#include "timers.h"

bool has_second_channel = false;

bool first_port_works =  false;
bool second_port_works = false;

bool enable_first_channel = true;
bool enable_second_channel = true;

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

    // printf("PS/2 config value %x\n", data);

    data &= ~0x43;
    outb(RW_PORT, CMD_CONFIG_WRITE);
    outb(DATA_PORT, data);

    if (data & 0x20) {
        has_second_channel = true; 
        printf("Info: PS/2 controller has second channel\n");
    }

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

        if (data & 0x20)
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
        first_port_works = enable_first_channel;
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
            second_port_works = enable_second_channel;
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

    request_priority(1);
    init_controller();

    if (!first_port_works && !second_port_works) {
        fprintf(stderr, "Error: No PS/2 ports apear to be useful...\n");
        exit(1);
    }

    if (second_port_works)
        port2_int = get_interrupt_number(12, int12_chan);

    if (first_port_works)
        port1_int = get_interrupt_number(1, int1_chan);

    enable_ports();

    init_ports();

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

        if (desc.size < IPC_MIN_SIZE) {
            fprintf(stderr, "Error: Message too small (size %i)\n", desc.size);
            free(message);
        }


        switch (IPC_TYPE(message)) {
        case IPC_Kernel_Interrupt_NUM: {
            if (desc.size < sizeof(IPC_Kernel_Interrupt)) {
                printf("Warning: message for type %i is too small (size %x)\n", IPC_Kernel_Interrupt_NUM, desc.size);
                break;
            }

            IPC_Kernel_Interrupt* str = (IPC_Kernel_Interrupt*)message;

            if (str->intno == port1_int) {
                react_port1_int();
                break;
            }

            if (str->intno == port2_int) {
                react_port2_int();
                break;
            }

            printf("Warning: Recieved unknown interrupt %i\n", str->intno);
        }
            break;
        case IPC_Timer_Reply_NUM: {
            unsigned expected_size = sizeof(IPC_Timer_Reply);
            if (desc.size != expected_size) {
                fprintf(stderr, "Warning: Recieved message of wrong size on channel %lx (expected %x got %x)\n", desc.channel, expected_size, desc.size);
                break;
            }

            IPC_Timer_Reply* reply = (IPC_Timer_Reply*)message;

            if (reply->type != IPC_Timer_Reply_NUM) {
                fprintf(stderr, "Warning: Recieved unexpected meesage of type %x on channel %lx\n", reply->type, desc.channel);
                break;
            }

            react_timer(reply->extra);            

            break;
        }
        default:
            fprintf(stderr, "Warning: Recieved message of unknown type %lx\n", IPC_TYPE(message));
            break;
        }

        free(message);
    }

    return 0;
}