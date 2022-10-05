#include <multiboot2.h>
#include <stdint.h>
#include <misc.h>
#include <io.h>
#include <entry.h>

int p_size;

void main(unsigned long magic, unsigned long addr)
{
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) return;

    /* Initialize everything */
    print_str("Hello from loader!\n");

  print_str("Executable size: ");
  print_hex(p_size);
  print_str("\n");
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
        case MULTIBOOT_TAG_TYPE_LOAD_BASE_ADDR: {
            struct multiboot_tag_load_base_addr * str = (struct multiboot_tag_load_base_addr *) tag;
            print_str("Load base address: ");
            print_hex(str->load_base_addr);
            print_str("\n");
        }
            break;
        default:
            break;
        }
      }
      init_mem(addr);
}