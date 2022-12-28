#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <string.h>
#include <pmos/system.h>
#include <stdio.h>
#include <acpi/acpi.h>
#include <stddef.h>
#include <pmos/special.h>
#include <pci/pci.h>
#include <ioapic/ioapic.h>
#include <devicesd/devicesd_msgs.h>
#include <interrupts/interrupts.h>
#include <configuration.h>
#include <timers/timers.h>
#include <timers/hpet.h>

char* exec = NULL;

void usage()
{
    printf("Usage: %s [args] - initializes and manages devices in pmOS.\n"
            " Supported arguments:\n"
            "   --rsdp-desc <addr> - sets RSDP physical address\n"
            "   --rsdp20-desc <addr> - sets RSDP 2.0 physical address\n", exec ? exec : "");
    exit(0);
}

void parse_args(int argc, char** argv)
{
    if (argc < 1) return;
    exec = argv[0];

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--rsdp-desc") == 0) {
            ++i;
            if (i == argc) {
                printf("Error: malformed options\n");
                usage();
            }
            rsdp_desc = (RSDP_descriptor*)strtoul(argv[i], NULL, 0);
        } else if (strcmp(argv[i], "--rsdp20-desc") == 0) {
            ++i;
            if (i == argc) {
                printf("Error: malformed options\n");
                usage();
            }
            rsdp20_desc = (RSDP_descriptor20*)strtoul(argv[i], NULL, 0);
        }
    }
}

#define CONTROL_PORT 10

int main(int argc, char** argv) {
    printf("Hello from devicesd!\n");
    
    parse_args(argc, argv);

    pmos_request_io_permission();

    if (rsdp_desc || rsdp20_desc)
        init_acpi();

    init_ioapic();
    init_timers();

    // TODO: Works, but needs PCI initialization first
    //if (acpi_revision != -1)
    //    init_lai();

    // Get messages
    result_t r = set_port_default(1024, getpid(), CONTROL_PORT);
    if (r != SUCCESS) printf("Warning: could not set the default port\n");


    while (1)
    {
        syscall_r r = block(MESSAGE_UNBLOCK_MASK);
        if (r.result != SUCCESS) break;

        switch (r.value) {
            case MESSAGE_S_NUM: // Unblocked by a message
            {
            Message_Descriptor msg;
            syscall_get_message_info(&msg);

            char* msg_buff = (char*)malloc(msg.size);

            get_first_message(msg_buff, 0);

            switch(msg.channel) {
                case CONTROL_PORT:
                {
                    if (msg.size >= sizeof(DEVICESD_MSG_GENERIC)) {
                        switch (((DEVICESD_MSG_GENERIC*)msg_buff)->type) {
                            case DEVICESD_MESSAGE_REG_INT_T: {
                                if (msg.size != sizeof(DEVICESD_MESSAGE_REG_INT))
                                    printf("Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);
                                // TODO: Add more checks & stuff

                                DEVICESD_MESSAGE_REG_INT* m = (DEVICESD_MESSAGE_REG_INT*)msg_buff;

                                uint8_t result = configure_interrupts_for(m);

                                DEVICESD_MESSAGE_REG_INT_REPLY reply;
                                reply.type = DEVICESD_MESSAGE_REG_INT_REPLY_T;
                                reply.status = result != 0;
                                reply.intno = result;
                                send_message_task(msg.sender, m->reply_chan, sizeof(reply), (char*)&reply);
                            }
                                break;
                            case DEVICESD_MESSAGE_TIMER_T: {
                                if (msg.size != sizeof(DEVICESD_MESSAGE_TIMER))
                                    printf("Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);

                                DEVICESD_MESSAGE_TIMER* m = (DEVICESD_MESSAGE_TIMER*)msg_buff;
                                start_timer(m->ms, m->extra, msg.sender, m->reply_channel);
                            }

                                break;
                            default:
                                printf("Warning: Recieved unknown message %x from PID %li\n", ((DEVICESD_MSG_GENERIC*)msg_buff)->type, msg.sender);    
                                break;
                            }
                        }
                    }
                    break;
                case hpet_int_chan: {
                        // TODO: Check that it's from kernel, etc.
                        hpet_int();
                    }
                        break;
                default:
                    break;
            }

            free(msg_buff);
        }
            break;
        default: // Do nothing
            break;
        }
    }

    return 0;
}