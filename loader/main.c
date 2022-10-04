#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>

void print_str(char * str)
{
    while (*str != '\0') { 
        printc(*str);
        ++str;
    }
    return;
}

void int_to_hex(char * buffer, long long n)
{
  char buffer2[24];
  int buffer_pos = 0;
  do {
    char c = n & 0x0f;
    if (c > 9) c = c - 9 + 'A';
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

void print_hex(long i)
{
    char buffer[24];
    print_str("0x");
    int_to_hex(buffer, i);
    print_str(buffer);
}

void main(unsigned long magic, unsigned long addr)
{
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) return;

    /* Initialize everything */
    print_str("Hello from loader!\n");


    print_str("Printing multiboot2 tags...\n");
    print_str("multiboo2 tags address: ");
    print_hex(addr);
    print_str(", ");
    unsigned long tag = addr + 8;
    long size = *(long*) addr;
    print_str("multiboot2 tags size: ");
    print_hex(size);
    print_str("\n");

    for (int i = 0; tag < addr + size && i < 20; ++i) {
        print_str("Tag ");
        print_hex(i);
        print_str(" type: ");
        print_hex(*(long*) tag);
        print_str(" with size: ");
        long t_size = *(long*)(tag + 4);
        print_hex(t_size);
        print_str("\n");
        tag += t_size;
    }
}