#include <multiboot2.h>
#include <linker.h>
#include <stdint.h>
#include <kernel/memory.h>
#include <io.h>
#include <mem.h>
#include <paging.h>

typedef struct {
    uint64_t base;
    uint64_t size;
} mem_entry;

uint64_t * bitmap = 0;
uint64_t bitmap_size;

mem_entry reserved[64];
int reserved_i = 0;

void bitmap_mark_bit(uint64_t pos, char b)
{
    uint64_t l = pos%64;
    uint64_t i = pos/64;

    if (b) bitmap[i] |= (uint64_t)0x01 << l;
    else bitmap[i] &= ~((uint64_t)0x01 << l);
}

char bitmap_read_bit(uint64_t pos)
{
    uint64_t l = pos%64;
    uint64_t i = pos/64;

    return !!(bitmap[i] & ((uint64_t)0x01 << l));
}

void mark(uint64_t base, uint64_t size, char usable)
{
    uint64_t base_page = base >> 12;
    uint64_t size_page = size >> 12;
    if (size_page < 64) {
        for (uint64_t i = 0; i < size_page; ++i) {
            bitmap_mark_bit(base_page + i, usable);
        }
    } else { // TODO: INEFFICIENT!
        uint64_t pattern = usable ? ~(uint64_t)0x0 : (uint64_t)0x0;
        while (base_page%64 && size_page > 0) {
            bitmap_mark_bit(base_page, usable);
            ++base_page;
            --size_page;
        }
        while (size_page > 64) {
            bitmap[base_page >> 6] = pattern;
            base_page += 64;
            size_page -= 64;
        }
        while (size_page > 0) {
            bitmap_mark_bit(base_page, usable);
            ++base_page;
            --size_page;
        }
    }
}

void mark_usable(uint64_t base, uint64_t size)
{
    mark(base, size, 1);
}

void reserve(uint64_t base, uint64_t size)
{
    reserved[reserved_i] = (mem_entry){base, size};
    reserved_i++;
}

void reserve_unal(uint64_t base, uint64_t size)
{
    uint64_t base_alligned = base & ~0xfff;
    size += base & 0xfff;
    uint64_t size_alligned = size & ~0xfff;
    if (size & 0xfff) size_alligned += 0x1000;

    if (size == 0) return;

    reserve(base_alligned, size_alligned);
}

void bitmap_reserve(uint64_t base, uint64_t size)
{
    mark(base, size, 0);
}

// Allocates a page
uint64_t alloc_page()
{
    uint64_t page = 0;
    // Find free page
    for (uint64_t i = 4/*0x100000 >> (12*6)*/; i < bitmap_size; ++i) {
        if (bitmap[i])
            for (int j = 0; j < 64; ++j) {
                if (bitmap_read_bit(i*64 + j)) {
                    page = i*64 + j;
                    goto skip;
                }
            }
    }
skip:
    bitmap_mark_bit(page, 0);
    page <<=12;

    return page;
}


void init_mem(unsigned long multiboot_str)
{
    //print_str("Initializing memory\n");
    // Find highest available memory entry
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

                    /*
                    print_str("Mem entry base ");
                    print_hex(base_addr);
                    print_str(" length ");
                    print_hex(length);
                    print_str(" type ");
                    print_hex(type);
                    print_str("\n");
                    */
                    

                    if (type == MULTIBOOT_MEMORY_AVAILABLE) {
                        if (base_addr > base) {
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
    uint64_t bitmap_phys = high_u;

    // Map the bitmap
    static const uint64_t bitmap_virt = 68719476736;
    for (uint64_t i = 0; i < bitmap_size*8; i += 4096) {
        Page_Table_Argumments arg = {0};
        arg.writeable = 1;
        arg.user_access = 1;
        arg.extra = 0b010;
        map(bitmap_virt + i, bitmap_phys + i, arg);
    }

    bitmap = (uint64_t*)bitmap_virt;

    reserve(high_u, bitmap_size*8);

    memclear((void*)bitmap, bitmap_size*8);

    for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MMAP) {
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
