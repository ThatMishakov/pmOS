#include <load_elf.h>
#include <syscall.h>
#include "../kernel/common/elf.h"
#include "../kernel/common/errors.h"
#include <utils.h>
#include <io.h>
#include <misc.h>
#include <mem.h>

uint64_t load_elf(uint64_t addr, uint8_t ring)
{
    ELF_64bit * elf_h = (ELF_64bit*)addr;
    if (elf_h->magic != 0x464c457f) {
        print_str("Error: not elf format!\n");
        return 1;
    }


    ELF_PHeader_64 * elf_pheader = (ELF_PHeader_64 *)((uint64_t)elf_h + elf_h->program_header);
    int elf_pheader_entries = elf_h->program_header_entries;

    static const uint64_t memory = 0x800000000;

    // get a PID
    syscall_r r = syscall_new_process(ring);
    if (r.result != SUCCESS) {
        print_str("Syscall failed!\n");
        return 1;
    }
    uint64_t pid = r.value;

    for (int i = 0; i < elf_pheader_entries; ++i) {
        ELF_PHeader_64 * p = &elf_pheader[i];
        if (p->type == ELF_SEGMENT_LOAD) {
            uint64_t phys_loc = (uint64_t)elf_h + p->p_offset;
            uint64_t vaddr = p->p_vaddr;
            uint64_t size = p->p_filesz;
            uint64_t pages = (p->p_memsz >> 12) + (pages & 0xfff ? 1 : 0);

            // TODO: Error checking
            syscall_r r = get_page_multi(memory, pages);
            print_str("get_page_multi() ");
            print_hex(pages);
            print_str(" ");
            print_hex(pages);
            print_str(" -> ");
            print_hex(r.result);
            print_str("\n");

            memcpy((char*)phys_loc, (char*)memory, size);

            Page_Table_Argumments arg = {};
            arg.user_access = 1;
            arg.writeable = p->flags & ELF_FLAG_WRITABLE;
            arg.execution_disabled = !(p->flags & ELF_FLAG_EXECUTABLE);

            r = map_into_range(pid, memory, vaddr, pages, arg);
            if (r.result != SUCCESS) {
                print_hex(r.result);
                print_str(" !!!\n");
                halt();
            }
        }
    }
    return 0;
}
