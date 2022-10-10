#include <mem.h>
#include <multiboot2.h>
#include <entry.h>
#include "../kernel/common/elf.h"
#include <utils.h>

void load_kernel(uint64_t multiboot_info_str)
{
    struct multiboot_tag_module * mod = 0;

    for (struct multiboot_tag * tag = (struct multiboot_tag *) (multiboot_info_str + 8); tag->type != MULTIBOOT_TAG_TYPE_END;
      tag = (struct multiboot_tag *) ((multiboot_uint8_t *) tag + ((tag->size + 7) & ~7))) {
        switch (tag->type) {
        case MULTIBOOT_TAG_TYPE_MODULE: {
            if (str_starts_with(((struct multiboot_tag_module *)tag)->cmdline, "kernel"))
                mod = (struct multiboot_tag_module *)tag;
        }
            break;
        default:
            break;
        }
      }

    if (!mod) {
        print_str("Did not find kernel module\n");
        return;
    }

    /* Parse elf */
    ELF_64bit * elf_h = (ELF_64bit*)mod->mod_start;
    if (elf_h->magic != 0x464c457f) {
        print_str("Kernel is not in ELF format\n");
        return;
    }

    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    print_str("ELF contents:\n");
    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        print_str(" Type: ");
        print_hex(p->type);
        print_str(" p_offset: ");
        print_hex(p->p_offset);
        print_str(" mem_size: ");
        print_hex(p->p_memsz);
        print_str("\n");
    }
}

