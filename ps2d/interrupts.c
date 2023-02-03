#include "interrupts.h"
#include <pmos/helpers.h>
#include <pmos/ipc.h>
#include <pmos/system.h>
#include <stdio.h>
#include <stdlib.h>
#include <kernel/block.h>

uint8_t get_interrupt_number(uint32_t intnum, uint64_t int_chan)
{
    uint8_t int_vector = 0;
    unsigned long mypid = getpid();

    IPC_Reg_Int m = {IPC_Reg_Int_NUM, IPC_Reg_Int_FLAG_EXT_INTS, intnum, 0, mypid, int_chan, interrupts_conf_reply_chan};
    result_t result = send_message_port(1024, sizeof(m), (char*)&m);
    if (result != SUCCESS) {
        printf("Warning: Could not send message to get the interrupt\n");
        return 0;
    }

    syscall_r r = block(MESSAGE_UNBLOCK_MASK);
    if (r.result != SUCCESS) {
        printf("Warning: Could not block\n");
        return 0;
    }

    Message_Descriptor desc = {};
    unsigned char* message = NULL;
    result = get_message(&desc, &message);

    if (result != SUCCESS) {
        printf("Warning: Could not get message\n");
        return 0;
    }

    if (desc.channel != 5) {
        printf("Warning: Recieved message from unknown channel %li\n", desc.channel);
        free(message);
        return 0;
    }

    IPC_Reg_Int_Reply* reply = (IPC_Reg_Int_Reply*)message;

    if (reply->status == 0) {
        printf("Warning: Did not assign the interrupt\n");
    } else {
        int_vector = reply->intno;
        printf("Info: Assigned interrupt %i\n", int_vector);
    }
    free(message);
    return int_vector;
}
