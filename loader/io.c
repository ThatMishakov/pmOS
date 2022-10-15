#include <io.h>
#include <misc.h>
#include <stdint.h>
#include <screen.h>

void print_str(char * str)
{
    while (*str != '\0') { 
        putchar(*str);
        ++str;
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