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
    if (c > 9) c = c - 10 + 'A';
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

void print_hex(long long i)
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


  struct multiboot_tag *tag;
  print_str("Multiboot tags location: ");
  print_hex(addr);
  print_str("\n");
  unsigned size;
  for (tag = (struct multiboot_tag *) (addr + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        print_str("Multiboot tag ");
        print_hex(tag->type);
        print_str(" size ");
        print_hex(tag->size);
        print_str("\n");

        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
            print_str("Memory lower: ");
            print_hex(((struct multiboot_tag_basic_meminfo *) tag)->mem_lower);
            print_str(" memory upper: ");
            print_hex(((struct multiboot_tag_basic_meminfo *) tag)->mem_upper);
            print_str("\n");
            break;
        case MULTIBOOT_TAG_TYPE_MMAP: {
            print_str("Memory map:\n");
            for (struct multiboot_mmap_entry * mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
                mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size)){
                    print_str("  Base addr: ");
                    print_hex(mmap->addr);
                    print_str(" length: ");
                    print_hex(mmap->len);
                    print_str(" type: ");
                    print_hex(mmap->type);
                    print_str("\n");
                }
        }
        case MULTIBOOT_TAG_TYPE_MODULE: {
            struct multiboot_tag_module * mod = (struct multiboot_tag_module *) tag;
            print_str("Module start: ");
            print_hex(mod->mod_start);
            print_str(" end: ");
            print_hex(mod->mod_end);
            print_str(" cmdline: ");
            print_str(mod->cmdline);
            print_str("\n");
        }
            break;
        default:
            break;
        }
      }
}