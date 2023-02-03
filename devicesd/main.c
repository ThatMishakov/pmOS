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
#include <pmos/ipc.h>
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
    request_priority(0);

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

            if (msg.size >= sizeof(DEVICESD_MSG_GENERIC)) {
                switch (((DEVICESD_MSG_GENERIC*)msg_buff)->type) {
                case IPC_Reg_Int_NUM: {
                    if (msg.size != sizeof(IPC_Reg_Int))
                        printf("Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);
                    // TODO: Add more checks & stuff

                    IPC_Reg_Int* m = (IPC_Reg_Int*)msg_buff;

                    uint8_t result = configure_interrupts_for(m);

                    IPC_Reg_Int_Reply reply;
                    reply.type = IPC_Reg_Int_Reply_NUM;
                    reply.status = result != 0;
                    reply.intno = result;
                    send_message_task(msg.sender, m->reply_chan, sizeof(reply), (char*)&reply);
                }
                    break;
                case IPC_Start_Timer_NUM: {
                    if (msg.size != sizeof(IPC_Start_Timer))
                        printf("Warning: Message from PID %lx does no have the right size (%lx)\n", msg.sender, msg.size);

                    IPC_Start_Timer* m = (IPC_Start_Timer*)msg_buff;
                    start_timer(m->ms, m->extra, msg.sender, m->reply_channel);
                }

                    break;
                case IPC_Kernel_Interrupt_NUM:
                    // TODO: Check that it's from kernel, etc.

                    hpet_int();
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