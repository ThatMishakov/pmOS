#include <io.h>
#include <misc.h>
#include <stdint.h>
#include <utils.h>
#include <syscall.h>
#include <utils.h>
#include <stddef.h>
#include <pmos/ipc.h>

char screen_buff[8192];
int buff_pos = 0;
int buff_ack = 0;
uint64_t log_port = 0;

void set_print_syscalls(uint64_t port)
{
    log_port = port;

    struct {
        uint32_t type;
        char buff[256];
    } msg_str = {0x40, {}};

    unsigned length = buff_pos - buff_ack;
    char *str = &screen_buff[buff_ack];

    for (unsigned i = 0; i < length; i += 256) {
        msg_str.type = 0x40;
        unsigned size = length - i > 256 ? 256 : length - i;
        memcpy(msg_str.buff, &str[i], size);

        syscall(SYSCALL_SEND_MSG_PORT, log_port, size + sizeof(uint32_t), &msg_str);
    }

    // syscall(SYSCALL_SEND_MSG_PORT, 1, buff_pos - buff_ack, &screen_buff[buff_ack]);
    buff_ack = buff_pos;
}

void print_str(char * str)
{
    print_str_n(str, strlen(str));
}

void print_str_n(char * str, int length)
{
    if (log_port) {
        struct {
            uint32_t type;
            char buff[256];
        } msg_str = {0x40, {}};

        for (unsigned i = 0; i < length; i += 256) {
            unsigned size = length - i > 256 ? 256 : length - i;
            memcpy(msg_str.buff, &str[i], size);

            syscall(SYSCALL_SEND_MSG_PORT, log_port, size+sizeof(uint32_t), &msg_str);
            // TODO: Error checking
        }
    }

    if (!log_port)
        while (*str != '\0') { 
            screen_buff[buff_pos++] = (*str);
            ++str;
            if (buff_pos == 8192) buff_pos = 0;
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
    print_str("0x");
    int_to_hex(buffer, i, 1);
    print_str(buffer);
}
