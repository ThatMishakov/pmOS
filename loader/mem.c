#include <multiboot2.h>

#define TEMP_MMAP_SIZE 4096 // 128 MB usable
long temp_mmap[TEMP_MMAP_SIZE/4] = {0};

void bitmap_mark(long* bitmap, long long start, long long up_to, char b)
{
    long long pos = start;
    while (pos < up_to)
    {
        if (!(pos & 0x1f) && up_to - pos >= 32) {
            *(bitmap + (pos >> 7)) = b ? 0xff : 0x00;
            pos += 32;
        } else {
            long long base = pos >> 7;
            long long shift = pos & 0x1f;
            if (b) {
                *(bitmap + base) |= 0x01 << 0x1f;
            } else {
                *(bitmap + base) &= ~(0x01 << 0x1f);
            }
            ++pos;
        }
    }
}

// Marks memory availability. Takes base, length pointers, limit (-1 disables it) and if to mark it as usable.
void mark_memory(long long base, long long length, long long limit, char usable)
{
    long long page = base >> 12;
    long long n_pages = length >> 12;
    if ((base + length) & 0x0fff) ++n_pages;
    if (limit != -1) {
        if (base <= limit ) return;
        if (base + length > limit) n_pages = limit >> 12;
    }
    bitmap_mark(temp_mmap, page, page + n_pages, usable);
}

void init_mem(unsigned long multiboot_str)
{
    // Walk multiboot structure to find useful memory
      for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MMAP: {
            for (struct multiboot_mmap_entry * mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
                mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size)){
                    long long base_addr = mmap->addr;
                    long long length = mmap->len;
                    int type = mmap->type;

                    if (type == MULTIBOOT_MEMORY_AVAILABLE) {
                        // Usable memory
                        mark_memory(base_addr, length, TEMP_MMAP_SIZE*4096*8, 1);
                    }
                }
        }
        default:
            break;
        }
    }

    // Reserve memory

    // Reserve mbi
    mark_memory(multiboot_str, *(unsigned int*)multiboot_str, TEMP_MMAP_SIZE*4096*8, 0);

}