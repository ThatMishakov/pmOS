#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <string.h>
#include <system.h>
#include "asm.hh"

void write_bochs(char* buff)
{
    while (*buff != '\0') {
        bochs_out(*buff);
        buff++;
    }
}

extern "C" int main() {
    write_bochs("Hello from terminald!\n");

    while (1)
    {
        syscall_r r = block(MESSAGE_UNBLOCK_MASK);
        if (r.result != SUCCESS) break;

        switch (r.value) {
        case MESSAGE_S_NUM: // Unblocked by a message
        {
            Message_Descriptor msg;
            syscall_get_message_info(&msg);

            char* msg_buff = (char*)malloc(msg.size+1);

            get_first_message(msg_buff, 0);

            msg_buff[msg.size] = '\0';

            write_bochs(msg_buff);
        }
            break;
        default: // Do nothing
            break;
        }
    }
    

    return 0;
}