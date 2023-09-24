#include <stdlib.h>
#include <kernel/types.h>
#include <kernel/errors.h>
#include <kernel/block.h>
#include <string.h>
#include <pmos/system.h>
#include "asm.hh"
#include <pmos/ipc.h>
#include <pmos/ports.h>

void putchar_screen (int c);
void write_screen(char* buff)
{
    while (*buff != '\0') {
        putchar_screen(*buff);
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
    write_screen(const_cast<char*>("0x"));
    int_to_hex(buffer, i, 1);
    write_screen(buffer);
}

void init_screen();

pmos_port_t main_port = 0;
pmos_port_t configuration_port = 0;

const char terminal_port_name[] = "/pmos/terminald";
const char stdout_port_name[] = "/pmos/stdout";
const char stderr_port_name[] = "/pmos/stderr";

int main() {
    // request_priority(0);
    init_screen();
    write_screen(const_cast<char*>("Hello from terminald!\n"));

    {
        ports_request_t req;
        req = create_port(PID_SELF, 0);
        if (req.result != SUCCESS) {
            write_screen("Error creating configuration port ");
            print_hex(req.result);
            write_screen("\n");
        }
        configuration_port = req.port;

        req = create_port(PID_SELF, 0);
        if (req.result != SUCCESS) {
            write_screen("Error creating main port ");
            print_hex(req.result);
            write_screen("\n");
        }
        main_port = req.port;
    }

    set_log_port(main_port, 0);

    {
        result_t r = name_port(main_port, terminal_port_name, strlen(terminal_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }

        r = name_port(main_port, stdout_port_name, strlen(stdout_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }

        r = name_port(main_port, stderr_port_name, strlen(stderr_port_name), 0);
        if (r != SUCCESS) {
            write_screen("terminald: Error 0x");
            print_hex(r);
            write_screen(" naming port\n");
        }
    }

    while (1)
    {
        Message_Descriptor msg;
        syscall_get_message_info(&msg, main_port, 0);

        char* msg_buff = (char*)malloc(msg.size+1);

        get_first_message(msg_buff, 0, main_port);

        msg_buff[msg.size] = '\0';

        if (msg.size < sizeof(IPC_Write_Plain)-1) {
            write_screen("Warning: recieved very small message\n");
            free(msg_buff);
            break;
        }

        IPC_Write_Plain *str = (IPC_Write_Plain *)(msg_buff);

        switch (str->type) {
        case IPC_Write_Plain_NUM:
            write_screen(str->data);
            break;
        default:
            write_screen("Warning: Unknown message type ");
            print_hex(str->type);
            write_screen("\n");
        }

        free(msg_buff);
    }
    

    return 0;
}