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

int main(int argc, char** argv) {
    printf("Hello from devicesd!\n");
    
    parse_args(argc, argv);

    pmos_request_io_permission();

    if (rsdp_desc || rsdp20_desc)
        init_acpi();

    init_ioapic();

    // TODO: Works, but needs PCI initialization first
    //if (acpi_revision != -1)
    //    init_lai();

    // Get messages
    result_t r = set_port_default(1024, getpid(), 10);
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
                default:
                    printf("Warning: Recieved unknown message %x from PID %li\n", ((DEVICESD_MSG_GENERIC*)msg_buff)->type, msg.sender);    
                    break;
                }
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