#include <multiboot2.h>
#include <linker.h>
#include <stdint.h>
#include <../kernel/common/memory.h>
#include <io.h>

#define TEMP_MMAP_SIZE 4096 // 128 MB usable
memory_descr temp_mem[TEMP_MMAP_SIZE/sizeof(memory_descr)];
int temp_mem_index = 0;

memory_descr *memm = temp_mem;
int *memm_index = &temp_mem_index;

void mark_usable(memory_descr* memmap, int* index, uint64_t base_addr, uint64_t size)
{
    uint64_t base_alligned = base_addr & ~0xfff;
    size += base_addr & 0xfff;
    uint64_t size_alligned = size & ~0xfff;
    //if (size & 0xfff) size_alligned += 0x1000;

    memmap[*index].base_addr = base_alligned;
    memmap[*index].size = size_alligned;
    ++*index;
}

void reserve(memory_descr* memmap, int* index, uint64_t base_addr, uint64_t size)
{
    uint64_t base_alligned = base_addr & ~0xfff;
    size += base_addr & 0xfff;
    uint64_t size_alligned = size & ~0xfff;
    if (size & 0xfff) size_alligned += 0x1000;

    if (size == 0) return;

    memory_descr temp[8];
    int t_index = 0;
    for (int i = 0; i < *index; ++i) { // TODO: assuming there is no memory overlap
        if (memmap[i].base_addr <= base_alligned && (memmap[i].base_addr + memmap[i].size > base_alligned)) {
            temp[t_index].base_addr = base_alligned + size_alligned;
            temp[t_index].size = memmap[i].base_addr + memmap[i].size - temp[t_index].base_addr;
            ++t_index;

            memmap[i].size = base_alligned - memmap[i].base_addr;
            break;
        }
    }
    for (int i = 0; i < t_index; ++i) {
        memmap[*index] = temp[i];
        ++*index;
    }
}

// Allocates a page
uint64_t alloc_page()
{
    for (int i = *memm_index - 1; i > 0; --i) {
        if (memm[i].size > 0) {
            memm[i].base_addr += 0x1000;
            memm[i].size -= 0x1000;
            print_str("Allocated ");
            print_hex(memm[i].base_addr - 0x1000);
            print_str("\n");

            return memm[i].base_addr - 0x1000;
        }
    }
    return -1;
}


void init_mem(unsigned long multiboot_str)
{
    print_str("Initializing memory\n");
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
                        mark_usable(temp_mem, &temp_mem_index, base_addr, length);
                    }
                }
        }
        default:
            break;
        }
    }

    // Reserve mbi
    reserve(temp_mem, &temp_mem_index, multiboot_str, *(unsigned int*)multiboot_str);

    // Reserve loader
    reserve(temp_mem, &temp_mem_index, (int)&_exec_start, (int)&_exec_size);

    // Reserve modules
    for (struct multiboot_tag *tag = (struct multiboot_tag *) (multiboot_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type)
        {
        case MULTIBOOT_TAG_TYPE_MODULE: {
            struct multiboot_tag_module * mod = (struct multiboot_tag_module *) tag;
            reserve(temp_mem, &temp_mem_index, mod->mod_start, mod->mod_end - mod->mod_start);
        }
        default:
            break;
        }
    }

    print_str("Memory stack:\n");
    for (int i = 0; i < temp_mem_index; ++i) {
        print_str("  Entry ");
        print_hex(i);
        print_str(": base: ");
        print_hex(temp_mem[i].base_addr);
        print_str(" size: ");
        print_hex(temp_mem[i].size);
        print_str("\n");
    }
}