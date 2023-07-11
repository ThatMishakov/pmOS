#include <pmos/ipc.h>
#include <stdio.h>
#include <pmos/ports.h>
#include <string.h>
#include <pmos/helpers.h>
#include <stdlib.h>

struct File_Descriptor root = NULL;

pmos_port_t main_port = 0;

const char *vfsd_port_name = "/pmos/vfsd";

int create_main_port()
{
    if (main_port != 0)
        return 0;

    ports_request_t request = create_port(PID_SELF, 0);
    if (request.result != SUCCESS) {
        printf("Error creating port %li\n", request.result);
        return -1;
    }

    main_port = request.port;

    return 0;
}

int name_port()
{
    result_t result = name_port(main_port, vfsd_port_name);
    if (result != SUCCESS) {
        printf("Error naming port %li\n", result);
        return -1;
    }

    return 0;
}

int main()
{
    printf("Hello from VFSd! My PID: %li\n", getpid());

    // Create the main port
    int result = create_main_port();
    if (result != 0)
        return 1;

    
    // Name the main port
    result = name_port();
    if (result != 0)
        return 1;


    // Get messages
    while (1) {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        char* msg_buff = (char*)malloc(msg.size);

        get_first_message(msg_buff, 0, main_port);

        if (msg.size >= sizeof(IPC_Generic_Msg)) {
            IPC_Generic_Msg* ipc_msg = (IPC_Generic_Msg*)msg_buff;

            switch (ipc_msg->type) {
            case IPC_Open_NUM: {
                printf("[VFSd] Recieved IPC_Open\n");

                IPC_Open* open_msg = (IPC_Open*)ipc_msg;
                

                break;
            }
            case IPC_Close_NUM: {
                printf("[VFSd] Recieved IPC_Close\n");
                IPC_Close* close_msg = (IPC_Close*)ipc_msg;
                break;
            }
            default:
                printf("[VFSd] Warning: Recieved unknown message type: %i\n", ipc_msg->type);
                break;
            }
        } else {
            printf("[VFSd] Warning: Recieved message that is too small. Size: %li\n", msg.size);
        }

        free(msg_buff);
    }

    return 0;
}