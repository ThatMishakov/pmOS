#include <multiboot2.h>
#include <linker.h>
#include <stdint.h>
#include <../kernel/common/memory.h>
#include <io.h>
#include <mem.h>

typedef struct {
    uint64_t base;
    uint64_t size;
} mem_entry;

mem_entry reserved[64];
int reserved_i = 0;

void init_mem(unsigned long multiboot_str)
{
    print_str("Initializing memory\n");
    // Find highes available memory entry
    uint64_t base = 0;
    uint64_t end = 0;

      for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MMAP: {
            for (struct multiboot_mmap_entry * mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
                mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size)) 
            {
                    uint64_t base_addr = mmap->addr;
                    uint64_t length = mmap->len;
                    int type = mmap->type;

                    if (type == MULTIBOOT_MEMORY_AVAILABLE) {
                        if ((base_addr < GB(1)) && (base_addr > base)) {
                            base = base_addr;
                            end = base + length;
                        }
                    }
                }
        }
        default:
            break;
        }
    }

    // Reserve mbi
    reserve_unal(multiboot_str, *(unsigned int*)multiboot_str);

    // Reserve loader
    reserve_unal((uint64_t)&_exec_start, (uint64_t)&_exec_size);

    // Reserve modules
    for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MODULE: {
            struct multiboot_tag_module * mod = (struct multiboot_tag_module *) tag;
            reserve_unal(mod->mod_start, mod->mod_end - mod->mod_start);
        }
        default:
            break;
        }
    }

    // Prepare bitmap
    uint64_t high_u = 0;
    for (int i = 0; i < reserved_i; ++i) {
        uint64_t end = reserved[i].base + reserved[i].size;
        if (end > high_u) high_u = end;
    }

    bitmap_size = end/4096/8/8;
    if (bitmap_size % 512) bitmap_size += 512 - bitmap_size % 512;
    bitmap = (uint64_t*)high_u;
    reserve(high_u, bitmap_size*8);

    memclear((void*)bitmap, bitmap_size*8);

    for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MMAP: {
            for (struct multiboot_mmap_entry * mmap = ((struct multiboot_tag_mmap *)tag)->entries;
                (multiboot_uint8_t *) mmap < (multiboot_uint8_t *) tag + tag->size;
                mmap = (multiboot_memory_map_t *) ((unsigned long) mmap + ((struct multiboot_tag_mmap *) tag)->entry_size)){
                    uint64_t base_addr = mmap->addr;
                    uint64_t length = mmap->len;
                    int type = mmap->type;

                    if (type == MULTIBOOT_MEMORY_AVAILABLE) {
                        mark_usable(base_addr, length);
                    }
                }
        }
        default:
            break;
        }
    }

    for (int i = 0; i < reserved_i; ++i)
        bitmap_reserve(reserved[i].base, reserved[i].size);
}

void memclear(void * base, int size_bytes)
{
    if (size_bytes < 16) {
        for (int i = 0; i < size_bytes; ++i)
            ((char*)base)[i] = 0;
    } else {
        char * p = (char*)base;
        char * limit = (char *)((uint64_t)p + size_bytes);
        while (((uint64_t)base & 0x0007) && p < limit) {
            *p = 0x00;
            ++p;
        }

        while ((uint64_t)base + 8 < (uint64_t)limit) {
            *((uint64_t*)base) = 0;
            base += 8;
        }

        while (p < limit) {
            *p = 0;
            ++p;
        }
    }
}

void memcpy(char * from, char * to, uint64_t bytes)
{
    // TODO: **EXTREMELY** INEFFICIENT
    while ((bytes--) > 0) {
        *to = *from;
        ++to; ++from;
    }   
}
