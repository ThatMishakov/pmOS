#include <mem.h>
#include <multiboot2.h>
#include <entry.h>
#include "../kernel/common/elf.h"
#include "../kernel/common/com.h"
#include <utils.h>
#include <misc.h>
#include <paging.h>
#include "include/mem.h"
#include <screen.h>
#include "io.h"

void load_kernel(uint64_t multiboot_info_str)
{
    print_str("Loading kernel\n");
    struct multiboot_tag_module * mod = 0;

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        if (tag->type == MULTIBOOT_TAG_TYPE_MODULE && str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "kernel"))
                mod = (struct multiboot_tag_module *)tag;
      }
    

    if (!mod) {
        print_str("Did not find kernel module\n");
        return;
    }

    /* Map kernel exec into memory */
    static uint64_t exec_virt_loc = 274877906944;
    for (uint64_t i = 0; i + mod->mod_start < mod->mod_end; i += 4096) {
        Page_Table_Argumments arg;
        arg.writeable = 1;
        arg.user_access = 1;
        arg.extra = 0b010;
        map(exec_virt_loc + i, mod->mod_start + i, arg);
    }

    /* Parse elf */
    ELF_64bit * elf_h = (ELF_64bit*)exec_virt_loc;
    if (elf_h->magic != 0x464c457f) {
        print_str("Kernel is not in ELF format\n");
        return;
    }

    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    print_str("==> Allocating memory...\n");
    // Allocate memory
    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            int pages = p->p_memsz >> 12;
            if (p->p_memsz & 0xfff) pages += 1;
            uint64_t base = p->p_vaddr;

            Page_Table_Argumments arg = {};
            arg.user_access = 0;
            arg.writeable = p->flags & ELF_FLAG_WRITABLE;
            arg.execution_disabled = !(p->flags & ELF_FLAG_EXECUTABLE);
            for (int i = 0; i < pages; ++i) {
                get_page(base + 0x1000*i, arg);
                clear_page(base + i*0x1000);
            }
        }
    }

    print_str("==> Loading executable\n");

    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t phys_loc = (uint64_t)elf_h + p->p_offset;
            uint64_t vaddr = p->p_vaddr;
            uint64_t size = p->p_filesz;
            memcpy((char*)phys_loc, (char*)vaddr, size);
        }
    }

    print_str("==> Jumping to kernel\n");

    int (*entry)(Kernel_Entry_Data*) = (void*)elf_h->program_entry;
    Kernel_Entry_Data data;
    data.mem_bitmap = bitmap;
    data.mem_bitmap_size = bitmap_size;
    data.temp_printc = &putchar;
    int r = entry(&data);
    print_str("==> Kernel returned ");
    print_hex(r);
    if (r == 0) print_str(" (success!)");
    print_str("\n");
}

