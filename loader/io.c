#include <io.h>
#include <misc.h>
#include <stdint.h>
#include <utils.h>
#include <syscall.h>
#include <utils.h>

char screen_buff[8192];
int buff_pos = 0;
int buff_ack = 0;
char write_to_system = 0;

void set_print_syscalls()
{
    write_to_system = 1;
    syscall(SYSCALL_SEND_MSG_PORT, 1, buff_pos - buff_ack, &screen_buff[buff_ack]);
    buff_ack = buff_pos;
}

void print_str(char * str)
{
    if (!write_to_system)
        while (*str != '\0') { 
            screen_buff[buff_pos++] = (*str);
            ++str;
            if (buff_pos == 8192) buff_pos = 0;
        }
    else {
        syscall(SYSCALL_SEND_MSG_PORT, 1, strlen(str), str);
    }
    return;
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

void printf(const char* format, ...)
{
    char ** arg = (char**) &format;
    char buff[20];
    ++arg;

    char c;
    while ((c = *(format++)) != '\0') {
        if (c != '%') printc(c);
        else {
            char *p;
            c = *(format++);
            switch (c) {
            case 'H':
            case 'h':
                int_to_hex(buff, *(long*)arg++, (c == 'H' ? 1 : 0));
                p = buff;
                goto string;
            case 's':
                p = *arg++;
                if (!p)
                    p = "(null)";
            string:
                print_str(p);
                break;
            case 'c':
                printc(*((char*)arg++));
                break;
            case '0':
                break;
            default:
                break;
            }
        }
    }
}
