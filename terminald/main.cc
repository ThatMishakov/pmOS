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

void int_to_hex(char * buffer, uint64_t n, char upper)
{
  char buffer2[24];
  int buffer_pos = 0;
  do {
    char c = n & 0x0f;
    if (c > 9) c = c - 10 + (upper ? 'A' : 'a');
    else c = c + '0';
    buffer2[buffer_pos] = c;
    n >>= 4;
    ++buffer_pos;
  } while (n > 0);

  for (int i = 0; i < buffer_pos; ++i) {
    buffer[i] = buffer2[buffer_pos - 1 - i];
  }
  buffer[buffer_pos] = '\0';
}

void print_hex(uint64_t i)
{
    char buffer[24];
    write_bochs(const_cast<char*>("0x"));
    int_to_hex(buffer, i, 1);
    write_bochs(buffer);
}

extern "C" int main() {
    write_bochs(const_cast<char*>("Hello from terminald!\n"));

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

            free(msg_buff);
        }
            break;
        default: // Do nothing
            break;
        }
    }
    

    return 0;
}