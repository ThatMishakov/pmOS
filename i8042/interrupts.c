#include "interrupts.h"
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/block.h>
#include <pmos/ports.h>

extern pmos_port_t configuration_port;
extern pmos_port_t main_port;
extern pmos_port_t devicesd_port;

uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_chan)
{
    uint8_t int_vector = 0;
    unsigned long mypid = getpid();

    IPC_Reg_Int m = {IPC_Reg_Int_NUM, IPC_Reg_Int_FLAG_EXT_INTS, intnum, 0, mypid, main_port, configuration_port};
    result_t result = send_message_port(devicesd_port, sizeof(m), (char*)&m);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    result = get_message(&desc, &message, configuration_port);

    if (result != SUCCESS) {
        printf("Warning: Could not get message\n");
        return 0;
    }

    if (desc.size < sizeof(IPC_Reg_Int_Reply)) {
        printf("Warning: Recieved message which is too small\n");
        free(message);
        return 0;
    }

    IPC_Reg_Int_Reply* reply = (IPC_Reg_Int_Reply*)message;

    if (reply->type != IPC_Reg_Int_Reply_NUM) {
        printf("Warning: Recieved unexepcted message type\n");
        free(message);
        return 0;
    }

    if (reply->status == 0) {
        printf("Warning: Did not assign the interrupt\n");
    } else {
        int_vector = reply->intno;
        printf("Info: Assigned interrupt %i\n", int_vector);
    }
    free(message);
    return int_vector;
}
